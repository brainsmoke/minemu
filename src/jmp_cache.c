
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
#include "error.h"
#include "jmp_cache.h"
#include "threads.h"

#define MAX_SEARCH 32

void add_jmp_mapping(char *addr, char *jit_addr)
{
	jmp_map_t *jmp_cache = get_thread_ctx()->jmp_cache;

	int hash = HASH_INDEX(addr), i;
	static int round_robin = 0;
	round_robin++; round_robin %= MAX_SEARCH;

	for (i=hash; i < hash+MAX_SEARCH; i++)
		if ( jmp_cache[i&0xffff].addr == NULL )
		{
			jmp_cache[i&0xffff] = (jmp_map_t) { .addr = CACHE_MANGLE(addr), .jit_addr = jit_addr };
			return;
		}

	jmp_cache[(hash+round_robin)&0xffff] = (jmp_map_t) { .addr = CACHE_MANGLE(addr), .jit_addr = jit_addr };
}

void clear_jmp_cache(thread_ctx_t *ctx, char *addr, unsigned long len)
{
	jmp_map_t *jmp_cache = ctx->jmp_cache;

	int i;
	char *tmp_addr, *tmp_jit_addr;

	for (i=0; i<JMP_CACHE_SIZE; i++)
	{
		jmp_map_t orig = jmp_cache[i];
		if ( contains(addr, len, CACHE_MANGLE(orig.addr)) )
			atomic_clear_8bytes((char*)&jmp_cache[i], (char*)&orig);
	}
}

char *find_jmp_mapping(char *addr)
{
	jmp_map_t *jmp_cache = get_thread_ctx()->jmp_cache;

	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<hash+MAX_SEARCH; i++)
		if ( (jmp_cache[i&0xffff].addr == CACHE_MANGLE(addr)) || (jmp_cache[i&0xffff].addr == NULL) )
			return jmp_cache[i&0xffff].jit_addr;

	return NULL;
}

