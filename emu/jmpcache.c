
#include "lib.h"
#include "error.h"
#include "jmpcache.h"
#include "scratch.h"

#define HASH_OFFSET(i, addr) (((unsigned long)(i)-(unsigned long)(addr))&0xfffful)

/* jump tables */

void add_jmp_mapping(char *addr, char *jit_addr)
{
	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<JMP_CACHE_SIZE; i++)
		if ( jmp_cache[i].addr == NULL )
		{
			jmp_cache[i] = (jmp_map_t) { .addr = addr, .jit_addr = jit_addr };
			return;
		}

	for (i=0; i<hash; i++)
		if ( jmp_cache[i].addr == NULL )
		{
			jmp_cache[i] = (jmp_map_t) { .addr = addr, .jit_addr = jit_addr };
			return;
		}

	debug("warning, hash jump table full");
	jmp_cache[hash] = (jmp_map_t) { .addr = addr, .jit_addr = jit_addr };

}

static void jmp_cache_clear(char *addr, unsigned long len)
{
	int i, last=-1 /* make compiler happy */ ;
	char *tmp_addr, *tmp_jit_addr;

	for (i=0; i<JMP_CACHE_SIZE; i++)
	{
		if ( contains(addr, len, jmp_cache[i].addr) )
			jmp_cache[i] = (jmp_map_t) { .addr = NULL, .jit_addr = NULL };

		if ( jmp_cache[i].addr == NULL )
			last = i;
	}

	for (i=0; i<JMP_CACHE_SIZE; i++)
	{
		if ( jmp_cache[i].addr == NULL )
			last = i;
		else if ( HASH_OFFSET(last, jmp_cache[i].addr) <
		          HASH_OFFSET(i, jmp_cache[i].addr) )
		{
			tmp_addr = jmp_cache[i].addr;
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
		if ( (jmp_cache[i].addr == addr) || (jmp_cache[i].addr == NULL) )
			return jmp_cache[i].jit_addr;

	for (i=0; i<hash; i++)
		if ( (jmp_cache[i].addr == addr) || (jmp_cache[i].addr == NULL) )
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

