
#include <string.h>

#include "lib.h"
#include "mm.h"

#include "jit.h"
#include "codemap.h"
#include "jmpcache.h"
#include "opcodes.h"
#include "error.h"
#include "runtime.h"

static char blocks[100];
static unsigned long n_blocks, block_size;

void jit_mm_init(void)
{
	block_size = 4*1024*1024;
	n_blocks = JIT_CODE_SIZE/block_size;
	memset(blocks, 0, n_blocks);
}

void *jit_alloc(unsigned long size)
{
	long i;

	if (size > block_size)
		die("requested block size too big");

	for (i=0; i<n_blocks; i++)
		if (blocks[i] == 0)
		{
			blocks[i] = 1;
			return (void *)(JIT_CODE_START+i*block_size);
		}

	die("out of blocks");
	return NULL;
}

void *jit_realloc(void *p, unsigned long size)
{
	if (p == NULL)
		return jit_alloc(size);

	if (size > block_size)
		die("requested block size too big");

	return p;
}

void jit_free(void *p)
{
	long i;

	if (!p)
		return;

	for (i=0; i<n_blocks; i++)
		if ( (JIT_CODE_START+i*block_size) == (unsigned long)p )
		{
			if (blocks[i] != 1)
				die("freeing unallocated block");

			blocks[i] = 0;
			return;
		}

	die("freeing noexistant block: %x", p);
}



