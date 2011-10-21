
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


#include "lib.h"
#include "error.h"
#include "jmp_cache.h"
#include "scratch.h"

#define HASH_OFFSET(i, addr) (((unsigned long)(i)-(unsigned long)(addr))&0xfffful)

/* jump tables */

void add_jmp_mapping(char *addr, char *jit_addr)
{
	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<JMP_CACHE_SIZE; i++)
		if ( jmp_cache[i].addr == NULL )
		{
			jmp_cache[i] = (jmp_map_t) { .addr = CACHE_MANGLE(addr), .jit_addr = jit_addr };
			return;
		}

	for (i=0; i<hash; i++)
		if ( jmp_cache[i].addr == NULL )
		{
			jmp_cache[i] = (jmp_map_t) { .addr = CACHE_MANGLE(addr), .jit_addr = jit_addr };
			return;
		}

	debug("warning, hash jump table full");
	jmp_cache[hash] = (jmp_map_t) { .addr = CACHE_MANGLE(addr), .jit_addr = jit_addr };

}

static void jmp_cache_clear(char *addr, unsigned long len)
{
	int i, last=-1 /* make compiler happy */ ;
	char *tmp_addr, *tmp_jit_addr;

	for (i=0; i<JMP_CACHE_SIZE; i++)
	{
		if ( contains(addr, len, CACHE_MANGLE(jmp_cache[i].addr)) )
			jmp_cache[i] = (jmp_map_t) { .addr = NULL, .jit_addr = NULL };

		if ( jmp_cache[i].addr == NULL )
			last = i;
	}

	for (i=0; i<JMP_CACHE_SIZE; i++)
	{
		if ( jmp_cache[i].addr == NULL )
			last = i;
		else if ( HASH_OFFSET(last, CACHE_MANGLE(jmp_cache[i].addr)) <
		          HASH_OFFSET(i,    CACHE_MANGLE(jmp_cache[i].addr)) )
		{
			tmp_addr = CACHE_MANGLE(jmp_cache[i].addr);
			tmp_jit_addr = jmp_cache[i].jit_addr;
			jmp_cache[i] = (jmp_map_t) { .addr = NULL, .jit_addr = NULL };
			add_jmp_mapping(tmp_addr, tmp_jit_addr);
			last = i;
		}

	}
}

char *find_jmp_mapping(char *addr)
{
	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<JMP_CACHE_SIZE; i++)
		if ( (jmp_cache[i].addr == CACHE_MANGLE(addr)) || (jmp_cache[i].addr == NULL) )
			return jmp_cache[i].jit_addr;

	for (i=0; i<hash; i++)
		if ( (jmp_cache[i].addr == CACHE_MANGLE(addr)) || (jmp_cache[i].addr == NULL) )
			return jmp_cache[i].jit_addr;

	return NULL;
}

void clear_jmp_mappings(char *addr, unsigned long len)
{
	jmp_cache_clear(addr, len);
}

/* only possible with some dynamic linking mechanism, probably not worth it anyway
void move_jmp_mappings(char *jit_addr, unsigned long jit_len, char *new_addr)
{
	int i;
	long diff = (long)new_addr-(long)jit_addr;

	for (i=0; i<JMP_CACHE_SIZE; i++)
		if ( !contains(jit_addr, jit_len, jmp_cache[i].jit_addr) )
			jmp_cache[i].jit_addr += diff;
}
*/

