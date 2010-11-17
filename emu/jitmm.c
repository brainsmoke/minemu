
#include <string.h>

#include "lib.h"
#include "mm.h"

#include "jit.h"
#include "codemap.h"
#include "jmp_cache.h"
#include "opcodes.h"
#include "error.h"
#include "runtime.h"

static short blocks[2000];
static long n_blocks;
static unsigned long block_size;

void jit_mm_init(void)
{
	block_size = 65536;
	n_blocks = JIT_CODE_SIZE/block_size;
	memset(blocks, 0, n_blocks*sizeof(short));
	blocks[0] = n_blocks;
}

void *jit_alloc(unsigned long size)
{
	long i, blocks_needed = size/block_size+1;

	if (blocks_needed < 1)
		blocks_needed = 1;

	for (i=0;i<n_blocks;)
	{
		if (blocks[i] < 0)
			i -= blocks[i];
		else if (blocks[i] < blocks_needed)
			i += blocks[i];
		else
		{
			if (blocks[i] > blocks_needed)
				blocks[i+blocks_needed] = blocks[i]-blocks_needed;

			blocks[i] = -blocks_needed;
			return (void *)(JIT_CODE_START+i*block_size);
		}
	}
	die("jit_alloc(): requested block size too big: %d", size);
	return NULL;
}

static int get_alloc_block(void *p)
{
	unsigned long offset = (unsigned long)p - JIT_CODE_START;

	if (offset % block_size)
		die("get_alloc_block(): bad pointer");

	int i = offset / block_size;

	if (i >= n_blocks)
		die("get_alloc_block(): bad pointer");

	if (blocks[i] >= 0)
		die("get_alloc_block(): unallocated block");

	return i;
}

void *jit_realloc(void *p, unsigned long size)
{
	if (p == NULL)
		return jit_alloc(size);

	if (size > blocks[get_alloc_block(p)]*block_size)
		die("requested block size too big");

	return p;
}

void jit_free(void *p)
{
	long this, next;

	if (!p)
		return;

	this = get_alloc_block(p);
	blocks[this] = -blocks[this];

	while ( (next=this+blocks[this]) < n_blocks && blocks[next] > 0 )
	{
		blocks[this] += blocks[next];
		blocks[next] = 0;
	}
}

