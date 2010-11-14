
#include <string.h>
#include <limits.h>

#include "lib.h"
#include "mm.h"

#include "jit.h"
#include "jitmm.h"
#include "codemap.h"
#include "jmp_cache.h"
#include "opcodes.h"
#include "jit_code.h"
#include "error.h"
#include "runtime.h"
#include "debug.h"
#include "syscalls.h"
#include "jit_cache.h"

#define TRANSLATED_MAX_SIZE (255)

unsigned long min(unsigned long a, unsigned long b) { return a<b ? a:b; }

/* jit code block layout:
 * (allocated in the jit code section of the address space)
 *
 * offset
 * -----------------------
 * 0x00                chunk header
 * sizeof(jit_chunk_t) jit code
 * hdr.tbl_off         translated instruction sizes
 *                     (a pair (orig size, jit size) of bytes per instruction)
 * hdr.chunk_len-2     terminating with ( 0, ... )
 * -----------------------
 * hdr.chunk_len       next chunk
 * ....
 * -----------------------
 * map.jit_len         end
 *
 */

/* We make sure that no fields in our jit code-structures use
 * absolute pointers into themselves. This way we can move pieces
 * of code more easily. (We only have to update the jump cache.)
 */

typedef struct
{
	char *addr; unsigned long len;
	unsigned long chunk_len, tree_off, tbl_off, n_ops;
	int tree_depth;

} jit_chunk_t;


/* a */
typedef struct
{
	unsigned char orig, jit;

} size_pair_t;

typedef struct
{
	char *addr;
	unsigned long off;

} rel_jmp_t;

/* Small min-heap implementation for relative jumps
 *
 * adding an element to the heap -> log n
 * removing the smallest element -> log n
 * with n relative to the number of jumps in the list
 *
 * During compilation all jumps are added and removed exaclty once,
 * which makes the worst case complexity n log n relative to the
 * number of jumps in the code.
 *
 */

typedef struct
{
	rel_jmp_t *buf;
	unsigned long size, max_size;

} jmp_heap_t;

/* create new empty heap using buffer buf */
void heap_init(jmp_heap_t *h, rel_jmp_t *buf, unsigned long buf_len)
{
	*h = (jmp_heap_t)
	{
		.buf = buf,
		.size = 0,
		.max_size = buf_len,
	};
}

/* puts the contents of *jmp in the heap */
void heap_put(jmp_heap_t *h, rel_jmp_t *jmp)
{
	if (h->size >= h->max_size)
		die("minemu: heap overflow");

	unsigned long i = h->size, parent = (i+1)/2-1;
	rel_jmp_t tmp;

	h->buf[i] = *jmp;
	h->size++;

	/* sift up */
	while ( (i > 0) &&
	        (unsigned long)h->buf[i].addr <
	        (unsigned long)h->buf[parent].addr )
	{
		tmp = h->buf[i];
		h->buf[i] = h->buf[parent];
		h->buf[parent] = tmp;

		i = parent;
		parent = (i+1)/2-1;
	}
}

/* Copies the relative jump with the smallest original address to *jmp,
 * removes it from the heap.
 *
 * Returns true unless the heap is empty in which case it returns false
 * and nothing is copied.
 */
int heap_get(jmp_heap_t *h, rel_jmp_t *jmp)
{
	if (h->size == 0)
		return 0;

	unsigned long i = 0, child = (i+1)*2-1;
	rel_jmp_t tmp;

	*jmp = h->buf[0];
	h->size--;
	h->buf[0] = h->buf[h->size];

	/* sift down */
	while (child < h->size)
	{
		/* choose child no. 2 if it has a lower address */
		if ( (child+1 < h->size) &&
		     ((unsigned long)h->buf[child+1].addr <
		      (unsigned long)h->buf[child].addr) )
			child+=1;

		if ( (unsigned long)h->buf[i].addr <
		     (unsigned long)h->buf[child].addr )
			break;

		tmp = h->buf[i];
		h->buf[i] = h->buf[child];
		h->buf[child] = tmp;

		i = child;
		child = (i+1)*2-1;
	}

	return 1;
}

/* address lookup
 * also called by runtime_ijmp in case of a cache miss (on a small stack)
 */
typedef struct
{
	unsigned long offset;
	unsigned long child;

} jit_node_entry_t;

typedef struct
{
	jit_node_entry_t entry[8];

} jit_node_t;

#define FRAME_SIZE (0x200)

static long jit_chunk_get_node_count(jit_chunk_t *hdr)
{
	unsigned long n_leafs = (hdr->chunk_len+FRAME_SIZE-1)/FRAME_SIZE, n_nodes = 0;
	hdr->tree_depth = 0;

	while ( n_leafs > 1 )
	{
		n_leafs = (n_leafs+7)/8;
		n_nodes += n_leafs;
		hdr->tree_depth++;
	}

	return n_nodes;
}

#define TREE_BRANCH (tree[path[cur_depth].node].entry[path[cur_depth].branch])
#define EMPTY ((unsigned long)-1)
#define ALIGN(x, n) ( ( (long)(x)+(n)-1 ) & ~( (n) -1 ) )
#define CHUNK_OFFSET(x) ( (long)(x) - (long)(hdr) )

static void jit_chunk_create_lookup_mapping(jit_chunk_t *hdr, size_pair_t *sizes)
{
	long n_nodes = jit_chunk_get_node_count(hdr), nodes_alloc=1, cur_depth=0, leftover;

	jit_node_t *tree = (jit_node_t *)ALIGN((long)hdr+hdr->chunk_len, 64);
	hdr->tree_off = CHUNK_OFFSET(tree);
	memset(tree, -1, n_nodes*sizeof(jit_node_t));

	struct { long node, branch; } path[hdr->tree_depth];
	memset(path, 0, sizeof(path));

	size_pair_t *map_addr = (size_pair_t *)&tree[n_nodes];
	hdr->tbl_off = CHUNK_OFFSET(map_addr);

	unsigned long n_ops = hdr->n_ops, cur_op=0, i=0,
	              s_off = 0, d_off = sizeof(*hdr);

	while (cur_op < n_ops)
	{
		if ( hdr->tree_depth > 0 )
		{
			while ( (cur_depth < hdr->tree_depth-1) || (TREE_BRANCH.child != EMPTY) )
			{
				if (TREE_BRANCH.child == EMPTY)
				{
					TREE_BRANCH = (jit_node_entry_t)
					{
						.offset = s_off,
						.child = (long)&tree[nodes_alloc]-(long)hdr,
					};

					cur_depth++;
					path[cur_depth].node = nodes_alloc;
					path[cur_depth].branch = 0;
					nodes_alloc++;
				
				}
				else if (path[cur_depth].branch < 7)
					path[cur_depth].branch++;
				else
					cur_depth--;
			}

			TREE_BRANCH = (jit_node_entry_t)
			{
				.offset = s_off,
				.child = (long)&map_addr[i]-(long)hdr,
			};
		}

		leftover = d_off & (FRAME_SIZE-1);
		if ( leftover > 0 )
		{
			map_addr[i] = (size_pair_t) { .orig = 0, .jit = leftover };
			i++;
		}

		do
		{
			map_addr[i] = sizes[cur_op];
			s_off += sizes[cur_op].orig;
			d_off += sizes[cur_op].jit;
			cur_op++; i++;
		}
		while ( (cur_op < n_ops) && ( (d_off-sizes[cur_op-1].jit) & FRAME_SIZE ) == ( d_off & FRAME_SIZE ) );
	}
	hdr->chunk_len = CHUNK_OFFSET(ALIGN(&map_addr[i], 64));
}

#undef TREE_BRANCH
#undef EMPTY
#undef ALIGN
#undef CHUNK_OFFSET

static char *jit_chunk_lookup_addr(jit_chunk_t *hdr, char *addr)
{
	if (!contains(hdr->addr, hdr->len, addr))
		return NULL;

	long d_off = 0, s_off = 0;
	jit_node_t *node = (jit_node_t *)((long)hdr+hdr->tree_off);
	int i, j;

	for (i=0; i<hdr->tree_depth; i++)
	{
		d_off *= 8;
		if ((unsigned long)addr-(unsigned long)hdr->addr < node->entry[0].offset)
			return NULL;

		for (j=0; j<7 && ((unsigned long)addr-(unsigned long)hdr->addr >= node->entry[j+1].offset); j++);

		s_off = node->entry[j].offset;
		d_off += FRAME_SIZE * j;
		node = (jit_node_t *)(((long)hdr)+node->entry[j].child);
	}

	size_pair_t *sizes = (size_pair_t *)node;

	j=0;
	if (sizes[j].orig == 0)
	{
		d_off += sizes[j].jit;
		j++;
	}

	while ((unsigned long)addr >= (unsigned long)&hdr->addr[s_off])
	{
		if ((unsigned long)addr == (unsigned long)&hdr->addr[s_off])
			return (char *)hdr+d_off;

		s_off += sizes[j].orig;
		d_off += sizes[j].jit;
		j++;
	}

	return NULL;
}

static char *jit_map_lookup_addr(code_map_t *map, char *addr)
{
	unsigned long off = 0;
	char *jit_addr;

	if (map->jit_addr == NULL)
		return NULL;

	while (off < map->jit_len)
	{
		jit_chunk_t *hdr = (jit_chunk_t *)&map->jit_addr[off];

		if ( (jit_addr = jit_chunk_lookup_addr(hdr, addr)) )
			return jit_addr;

		off += hdr->chunk_len;
	}

	return NULL;
}

char *jit_lookup_addr(char *addr)
{
	code_map_t *map = find_code_map(addr);
	char *jit_addr = NULL;

	if (map)
		jit_addr = jit_map_lookup_addr(map, addr);

	if (jit_addr)
		add_jmp_mapping(addr, jit_addr);

	return jit_addr;
}

/* reverse address lookup */

static char *jit_chunk_rev_lookup_addr(jit_chunk_t *hdr, char *jit_addr, char **jit_op_start, long *jit_op_len)
{
	if (!contains((char *)hdr, hdr->chunk_len, jit_addr))
		return NULL;

	unsigned long n_ops=hdr->n_ops, i, j;
	size_pair_t *sizes = (size_pair_t *)&((char *)hdr)[hdr->tbl_off];
	char *jit_addr_iter = (char *)&hdr[1], *addr=hdr->addr;

	for (i=0, j=0; i<n_ops; i++, j++)
	{
		if ( sizes[j].orig == 0 )
			j++;

		if (contains(jit_addr_iter, sizes[j].jit, jit_addr))
		{
			if (jit_op_start)
				*jit_op_start = jit_addr_iter;
			if (jit_op_len)
				*jit_op_len = sizes[j].jit;
			return addr;
		}

		addr += sizes[j].orig;
		jit_addr_iter += sizes[j].jit;
	}

	return NULL;
}

static char *jit_map_rev_lookup_addr(code_map_t *map, char *jit_addr, char **jit_op_start, long *jit_op_len)
{
	unsigned long off = 0;
	char *addr;

	while (off < map->jit_len)
	{
		jit_chunk_t *hdr = (jit_chunk_t *)&map->jit_addr[off];

		if ( (addr = jit_chunk_rev_lookup_addr(hdr, jit_addr, jit_op_start, jit_op_len)) )
			return addr;

		off += hdr->chunk_len;
	}

	return NULL;
}

char *jit_rev_lookup_addr(char *jit_addr, char **jit_op_start, long *jit_op_len)
{
	code_map_t *map = find_jit_code_map(jit_addr);

	if (map)
		return jit_map_rev_lookup_addr(map, jit_addr, jit_op_start, jit_op_len);

	return NULL;
}

/*  */

static void jit_chunk_fill_mapping(code_map_t *map, jit_chunk_t *hdr,
                                   unsigned long *mapping)
{
	unsigned long n_ops = hdr->n_ops,
	              i,j,
	              jit_off = (char *)&hdr[1] - map->jit_addr,
	              orig_off = hdr->addr - map->addr;

	size_pair_t *sizes = (size_pair_t *)&((char *)hdr)[hdr->tbl_off];

	for (i=0,j=0; i<n_ops; i++,j++)
	{
		if ( sizes[j].orig == 0 )
			j++;

		mapping[orig_off] = jit_off;
		orig_off += sizes[j].orig;
		jit_off += sizes[j].jit;
	}
}

static void jit_fill_mapping(code_map_t *map, unsigned long *mapping,
                                              unsigned long mapsize)
{
	unsigned long off = 0;
	memset(mapping, 0, mapsize*sizeof(char *));

	while (off < map->jit_len)
	{
		jit_chunk_t *hdr = (jit_chunk_t *)&map->jit_addr[off];
		jit_chunk_fill_mapping(map, hdr, mapping);
		off += hdr->chunk_len;
	}
}

static char *jit_map_resize(code_map_t *map, unsigned long new_len)
{
	char *new_addr = jit_realloc(map->jit_addr, new_len);

	if ( (map->jit_addr) && (new_addr != map->jit_addr) )
		die("jmp mapping resize is not supported");
//		move_jmp_mappings(map->jit_addr, map->jit_len, new_addr);

	map->jit_addr = new_addr;
	map->jit_len = new_len;

	return new_addr;
}

#define CHUNK_INCREASE (0x1000)

#define TRANSLATED(m) ((unsigned long)(m+0x1000)>0x1000)
/*#define UNTRANSLATED  ((unsigned long) 0)*/
#define NEEDED        ((unsigned long)-1)

static int try_resolve_jmp(code_map_t *map, char *jmp_addr, char *imm_addr,
                           unsigned long *mapping)
{
	if ( TRANSLATED(mapping[jmp_addr-map->addr]) )
	{
		long diff = (long)&map->jit_addr[mapping[jmp_addr-map->addr]] -
		            (long)imm_addr - 4;
		imm_to(imm_addr, diff);
		return 1;
	}
	else
		return 0;
}

/* Translate a chunk of chunk of code
 *
 */
static jit_chunk_t *jit_translate_chunk(code_map_t *map, char *entry_addr,
                                        jmp_heap_t *jmp_heap, unsigned long *mapping)
{
	char *jit_addr=map->jit_addr, *addr=map->addr;
	unsigned long n_ops = 0,
	              entry = entry_addr-addr,
	              s_off = entry_addr-addr,
	              d_off = map->jit_len+sizeof(jit_chunk_t),
	              chunk_base = map->jit_len;
	int stop = 0;

	instr_t instr;
	trans_t trans;
	rel_jmp_t jmp;
	size_pair_t sizes[map->len];

	while (stop == 0)
	{
		if ( map->jit_len < d_off+TRANSLATED_MAX_SIZE )
			jit_addr=jit_map_resize(map, d_off+CHUNK_INCREASE);

		mapping[s_off] = d_off;
		stop = read_op(&addr[s_off], &instr, map->len-s_off);
		translate_op(&jit_addr[d_off], &instr, &trans, map->addr, map->len);

		/* try to resolve translated jumps early */
		if ( (trans.imm != 0) && !try_resolve_jmp(map, trans.jmp_addr,
		                                          &jit_addr[d_off+trans.imm],
		                                          mapping) )
		{
			/* destination address not translated yet */
			jmp = (rel_jmp_t){ .addr=trans.jmp_addr, .off=d_off+trans.imm };
			heap_put(jmp_heap, &jmp);

			if (TRANSLATED(mapping[trans.jmp_addr-map->addr]))
				die("minemu bug");
			/* mark address as destination of jump
			 * to know whether to stop translation of code after
			 * unconditional jumps & returns
			 */
			mapping[trans.jmp_addr-map->addr] = NEEDED;
		}

		d_off += trans.len;
		s_off += instr.len;
		sizes[n_ops] = (size_pair_t) { instr.len, trans.len };

		if ( TRANSLATED(mapping[s_off]) )
		{
			stop = 1;
			generate_jump(&jit_addr[d_off], &addr[s_off], &trans,
			              map->addr, map->len);

			if (trans.imm != 0)
				if (!try_resolve_jmp(map, trans.jmp_addr,
				                     &jit_addr[d_off+trans.imm], mapping))
					die("minemu: assertion failed in jit_translate_chunk()");

			d_off += trans.len;
		}

		n_ops++;
	}

	jit_chunk_t *hdr = (jit_chunk_t*)&jit_addr[chunk_base];
	*hdr = (jit_chunk_t)
	{
		.addr = &addr[entry],
		.len = s_off-entry,
		.chunk_len = d_off-chunk_base, /* to be extrended during lookup map creation */
		.n_ops = n_ops,
	};

	jit_chunk_create_lookup_mapping(hdr, sizes);
	jit_map_resize(map, chunk_base+hdr->chunk_len);

	return hdr;
}

/* Translates all reachable code from map starting from entry_addr
 *
 */
static void jit_translate(code_map_t *map, char *entry_addr)
{
	jmp_heap_t jmp_heap;
	rel_jmp_t j;
	rel_jmp_t jumps[map->len]; /* mostly unused */
	unsigned long mapping[map->len+1]; /* waste of memory :-( */

	heap_init(&jmp_heap, jumps, map->len+1);

	jit_fill_mapping(map, mapping, map->len+1);

	jit_translate_chunk(map, entry_addr, &jmp_heap, mapping);

	while (heap_get(&jmp_heap, &j))
		while (!try_resolve_jmp(map, j.addr, &map->jit_addr[j.off], mapping))
			jit_translate_chunk(map, j.addr, &jmp_heap, mapping);
}

void jit_init(void)
{
	jit_mm_init();
}

char *jit(char *addr)
{
	char *jit_addr = find_jmp_mapping(addr);

	if (jit_addr != NULL)
		return jit_addr;

	code_map_t *map = find_code_map(addr);

	if (map->jit_addr == NULL)
	{
		map->jit_addr = jit_realloc(map->jit_addr, map->len*6);

		try_load_jit_cache(map);
	}

	if (map == NULL)
{
#ifdef EMU_DEBUG
char *jit_op, *op;
long jit_op_len, op_len;
op = jit_rev_lookup_addr(last_jit, &jit_op, &jit_op_len);
op_len = op_size(op, 16);
		debug("last opcode at: %X %d", op, op_len);
		printhex(op, op_len);
		debug("last jit opcode at: %X ", last_jit);
		printhex(jit_op, jit_op_len);
#endif
		die("attempting to jump in non-executable code addr: %X ", addr);
}
	jit_addr = jit_lookup_addr(addr);

	if (jit_addr == NULL)
	{
		jit_translate(map, addr);
		jit_addr = jit_lookup_addr(addr);

		try_save_jit_cache(map);
	}

	if (jit_addr == NULL)
		die("jit failed");

	return jit_addr;
}

