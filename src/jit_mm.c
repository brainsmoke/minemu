
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <string.h>

#include "lib.h"
#include "mm.h"
#include "syscalls.h"

#include "jit.h"
#include "codemap.h"
#include "jmp_cache.h"
#include "opcodes.h"
#include "error.h"
#include "runtime.h"

#define BLOCK_SIZE 65536

/* Blocks[i] is negative for blocks i which are the start
 * of an allocated region, -blocks[i] gives the number of blocks
 * allocated.
 *
 * Blocks[i] is positive for the start of an unallocated
 * region and gives the number of free blocks until the next
 * allocated region (or until the end of JIT code memory.)
 */
static short blocks[JIT_SIZE/BLOCK_SIZE + 1];
static long n_blocks;
static unsigned long block_size;

void jit_mem_init(void)
{
	block_size = BLOCK_SIZE;
	n_blocks = JIT_SIZE/block_size;
	memset(blocks, 0, sizeof(blocks));
	blocks[0] = n_blocks;
	blocks[n_blocks] = 0;
}

void print_jit_stats(void)
{
	long i;
	for (i=0;i<n_blocks;)
	{
		if (blocks[i] < 0)
		{
			debug("Taken: %x", -blocks[i]*block_size);
			i -= blocks[i];
		}
		else
		{
			debug("Free: %x", blocks[i]*block_size);
			i += blocks[i];
		}
	}
}

static int get_alloc_block(void *p)
{
	unsigned long offset = (unsigned long)p - JIT_START;

	if (offset % block_size)
		die("get_alloc_block(): bad pointer");

	int i = offset / block_size;

	if (i >= n_blocks)
		die("get_alloc_block(): bad pointer");

	if (blocks[i] >= 0)
		die("get_alloc_block(): unallocated block");

	return i;
}

static void *get_alloc_pointer(long i)
{
	if (i>=n_blocks)
		die("get_alloc_pointer(): bad block index: %d", i);

	return (void *)(JIT_START + i * block_size);
}

static void use_blocks(long i, long count)
{
	long ret = sys_mprotect(get_alloc_pointer(i), count*block_size,
	   	                    PROT_READ|PROT_WRITE);

	if (ret & PG_MASK)
		die("use_blocks(): mprotect: %d", ret);
}

static void disuse_blocks(long i, long count)
{
	long ret = sys_mmap2(get_alloc_pointer(i), count*block_size,
	   	                 PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (ret & PG_MASK)
		die("disuse_blocks(): mmap: %d", ret);
}

static long get_max_index(void)
{
	long max_blocks = 0, max_index = -1, i;

	for (i=0;i<n_blocks;)
	{
		if (blocks[i] > 0)
		{
			if (blocks[i] > max_blocks)
			{
				max_blocks = blocks[i];
				max_index = i;
			}
			i += blocks[i];
		}
		else
			i -= blocks[i];
	}

	return max_index;
}

void *jit_mem_balloon(void *p)
{
	long base, new;

	if (p)
	{
		base = get_alloc_block(p);
		new = base + -blocks[base];
	}
	else
	{
		base = new = get_max_index();
		if (base == -1)
			return NULL;
		
		p = get_alloc_pointer(base);
	}

	if (blocks[new] > 0)
	{
		use_blocks(new, blocks[new]);
		long newblocks = blocks[new];
		blocks[new] = 0;
		blocks[base] -= newblocks;
	}

	return p;
}

unsigned long jit_mem_size(void *p)
{
	return -blocks[get_alloc_block(p)]*block_size;
}

unsigned long jit_mem_try_resize(void *p, unsigned long requested_size)
{
	long base, next, newnext;
	base = get_alloc_block(p);
	next = base + -blocks[base];

	long diff = (requested_size+block_size+(requested_size?-1:0))/block_size -
	            -blocks[base];

	newnext = next + diff;

	if ( diff < 0 )
	{
		blocks[newnext] = -diff;
		disuse_blocks(newnext, -diff);
		if (blocks[next] > 0) /* next region is free space */
		{
			blocks[newnext] += blocks[next];
			blocks[next] = 0;
		}
		blocks[base] -= diff;
	}

	if ( diff > 0 )
	{
		if (blocks[next] > 0) /* next region is free space */
		{
			if (blocks[next] < diff)
				diff = blocks[next]; /* since it's best effort */

			use_blocks(next, diff);
			blocks[newnext] = blocks[next] - diff;
			blocks[next] = 0;
			blocks[base] -= diff;
		}
	}

	return -blocks[base]*block_size;
}

void jit_mem_free(void *p)
{
	if (!p)
		return;

	long base, next;
	base = get_alloc_block(p);
	blocks[base] = -blocks[base];
	disuse_blocks(base, blocks[base]);

	while ( (next=base+blocks[base]) < n_blocks && blocks[next] > 0 )
	{
		blocks[base] += blocks[next];
		blocks[next] = 0;
	}
}

