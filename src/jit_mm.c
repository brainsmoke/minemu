
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

static short blocks[JIT_SIZE/BLOCK_SIZE];
static long n_blocks;
static unsigned long block_size;

void jit_mm_init(void)
{
	block_size = BLOCK_SIZE;
	n_blocks = JIT_SIZE/block_size;
	memset(blocks, 0, n_blocks*sizeof(short));
	blocks[0] = n_blocks;
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
			return (void *)(JIT_START+i*block_size);
		}
	}
	print_jit_stats();
	die("jit_alloc(): requested block size too big: %d", size);
	return NULL;
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

unsigned long jit_size(void *p)
{
	return -blocks[get_alloc_block(p)]*block_size;
}

unsigned long jit_resize(void *p, unsigned long newsize)
{
	long blocks_needed = newsize/block_size+1;
	long i = get_alloc_block(p);

	blocks_needed -= -blocks[i];

	if ( blocks_needed < 0 )
	{
		blocks[i + -blocks[i] - -blocks_needed] = -blocks_needed;
		if (blocks[i + -blocks[i]] > 0)
		{
			blocks[i + -blocks[i] - -blocks_needed] += blocks[i + -blocks[i]];
			blocks[i + -blocks[i]] = 0;
		}
	}

	if ( blocks_needed > 0 )
	{
		if (blocks[i + -blocks[i]] >= blocks_needed)
		{
			blocks[i + -blocks[i] + blocks_needed] = blocks[i + -blocks[i]] - blocks_needed;
			blocks[i + -blocks[i]] = 0;
			sys_mmap2(&((char *)p)[-blocks[i]*block_size], blocks_needed*block_size,
			          PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
		}
		else
			die("cannot resize current jit block");
	}

	blocks[i] -= blocks_needed;
		
	return -blocks[get_alloc_block(p)]*block_size;
}

void jit_free(void *p)
{
	long this, next;

	if (!p)
		return;

	sys_mmap2(p, jit_size(p), PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	this = get_alloc_block(p);
	blocks[this] = -blocks[this];

	while ( (next=this+blocks[this]) < n_blocks && blocks[next] > 0 )
	{
		blocks[this] += blocks[next];
		blocks[next] = 0;
	}
}

