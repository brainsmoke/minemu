
#include "lib.h"
#include "error.h"
#include "jmpcache.h"
#include "scratch.h"

/* jump tables */

static char *jmp_cache_source[JMP_CACHE_SIZE];
static char *stub[JMP_CACHE_SIZE];
static long jmp_cache_size=0;

void print_jmp_list(void)
{
	int i;
	debug("jmp_list:");
	for (i=0; i<JMP_LIST_SIZE; i++) if (jmp_list.addr[i])
		debug("%x -> %x", jmp_list.addr[i], jmp_list.jit_addr[i]);
	debug("");
}

extern jmp_list_t tmp_list;
void print_jmp_listdiff(void)
{
	int i;
	for (i=0; i<JMP_LIST_SIZE; i++)
		if (!jmp_list.addr[i] || !jmp_list.jit_addr[i])
if (tmp_list.addr[i] || tmp_list.jit_addr[i])
		debug("%x -> %x (was %x -> %x)", jmp_list.addr[i], jmp_list.jit_addr[i], tmp_list.addr[i], tmp_list.jit_addr[i]);
//		if ( (jmp_list.addr[i] != tmp_list.addr[i]) ||
//		     (jmp_list.jit_addr[i] != tmp_list.jit_addr[i]) )
//if (tmp_list.addr[i] && tmp_list.jit_addr[i])
//		debug("%x -> %x (was %x -> %x)", jmp_list.addr[i], jmp_list.jit_addr[i], tmp_list.addr[i], tmp_list.jit_addr[i]);
	tmp_list = jmp_list;
}

void add_jmp_mapping(char *addr, char *jit_addr)
{
	int hash = HASH_INDEX(addr), i;
print_jmp_listdiff();

	for (i=hash; i<JMP_LIST_SIZE; i++)
		if ( jmp_list.addr[i] == NULL )
		{
			jmp_list.addr[i] = addr;
			jmp_list.jit_addr[i] = jit_addr;
debug("%04x %08x", i, addr);
			return;
		}

	for (i=0; i<hash; i++)
		if ( jmp_list.addr[i] == NULL )
		{
			jmp_list.addr[i] = addr;
			jmp_list.jit_addr[i] = jit_addr;
debug("%04x %08x", i, addr);
			return;
		}

	debug("warning, hash jump table full");
	jmp_list.addr[hash] = addr;
	jmp_list.jit_addr[hash] = jit_addr;

}

static void jmp_list_clear(char *addr, unsigned long len)
{
	int i, last;
	char *tmp_addr, *tmp_jit_addr;

	for (i=0; i<JMP_LIST_SIZE; i++)
	{
		if ( contains(addr, len, jmp_list.addr[i]) )
			jmp_list.addr[i] = jmp_list.jit_addr[i] = NULL;

		if ( jmp_list.addr[i] == NULL )
			last = i;
	}

	for (i=0; i<JMP_LIST_SIZE; i++)
	{
		if ( jmp_list.addr[i] == NULL )
			last = i;
		else if ( HASH_OFFSET(last, jmp_list.addr[i]) <
		          HASH_OFFSET(i, jmp_list.addr[i]) )
		{
			tmp_addr = jmp_list.addr[i];
			tmp_jit_addr = jmp_list.jit_addr[i];
			jmp_list.addr[i] = jmp_list.jit_addr[i] = NULL;
			add_jmp_mapping(tmp_addr, tmp_jit_addr);
			last = i;
		}

	}
}

char *find_jmp_mapping(char *addr)
{
	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<JMP_LIST_SIZE; i++)
		if ( (jmp_list.addr[i] == addr) || (jmp_list.addr[i] == NULL) )
			return jmp_list.jit_addr[i];

	for (i=0; i<hash; i++)
		if ( (jmp_list.addr[i] == addr) || (jmp_list.addr[i] == NULL) )
			return jmp_list.jit_addr[i];

	return NULL;
}

static void jmp_cache_clear(char *addr, unsigned long len)
{
	int i;

	for (i=0; i<jmp_cache_size; i++)
		if (contains(addr, len, jmp_cache_source[i]))
		{
			jmp_cache_source[i] = NULL;
			jmp_cache[i] = (jmp_map_t){ NULL, NULL };
			stub[i] = NULL;
		}

	for (i=0; i<jmp_cache_size; i++)
		if (contains(addr, len, jmp_cache[i].addr))
		{
			if (stub[i])
				jmp_cache[i].jit_addr = stub[i];
			else
				jmp_cache[i] = (jmp_map_t){ NULL, NULL };
		}
}

void clear_jmp_mappings(char *addr, unsigned long len)
{
	jmp_cache_clear(addr, len);
	jmp_list_clear(addr, len);
}

void move_jmp_mappings(char *jit_addr, unsigned long jit_len, char *new_addr)
{
	int i;
	long diff = (long)new_addr-(long)jit_addr;

	for (i=0; i<JMP_LIST_SIZE; i++)
		if ( !contains(jit_addr, jit_len, jmp_list.jit_addr[i]) )
			jmp_list.jit_addr[i] += diff;

	for (i=0; i<jmp_cache_size; i++)
		if ( !contains(jit_addr, jit_len, jmp_cache[i].jit_addr) )
			jmp_cache[i].jit_addr += diff;

	for (i=0; i<jmp_cache_size; i++)
		if ( !contains(jit_addr, jit_len, stub[i]) )
			stub[i] += diff;
}

char **alloc_stub_cache(char *src_addr, char *jmp_addr, char *stub_addr)
{
	int i;

	for (i=0; jmp_cache_source[i]; i++);
		if (i == JMP_CACHE_SIZE-1)
			die("jump cache overflow");

	jmp_cache_source[i] = src_addr;
	stub[i] = stub_addr;
	jmp_cache[i] = (jmp_map_t){ jmp_addr, stub_addr };

	if (jmp_cache_size < i+1)
		jmp_cache_size = i+1;

	return &jmp_cache[i].addr;
}

char **alloc_jmp_cache(char *src_addr)
{
	return alloc_stub_cache(src_addr, NULL, NULL);
}
