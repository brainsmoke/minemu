
#include "lib.h"
#include "error.h"
#include "jmpcache.h"
#include "scratch.h"

#define HASH_OFFSET(i, addr) (((unsigned long)(i)-(unsigned long)(addr))&0xfffful)

/* jump tables */

void add_jmp_mapping(char *addr, char *jit_addr)
{
	int hash = HASH_INDEX(addr), i;

	for (i=hash; i<JMP_LIST_SIZE; i++)
		if ( jmp_list.addr[i] == NULL )
		{
			jmp_list.addr[i] = addr;
			jmp_list.jit_addr[i] = jit_addr;
			return;
		}

	for (i=0; i<hash; i++)
		if ( jmp_list.addr[i] == NULL )
		{
			jmp_list.addr[i] = addr;
			jmp_list.jit_addr[i] = jit_addr;
			return;
		}

	debug("warning, hash jump table full");
	jmp_list.addr[hash] = addr;
	jmp_list.jit_addr[hash] = jit_addr;

}

static void jmp_list_clear(char *addr, unsigned long len)
{
	int i, last=-1 /* make compiler happy */ ;
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

void clear_jmp_mappings(char *addr, unsigned long len)
{
	jmp_list_clear(addr, len);
}

/* only possible with some dynamic linking mechanism, probably not worth it anyway
void move_jmp_mappings(char *jit_addr, unsigned long jit_len, char *new_addr)
{
	int i;
	long diff = (long)new_addr-(long)jit_addr;

	for (i=0; i<JMP_LIST_SIZE; i++)
		if ( !contains(jit_addr, jit_len, jmp_list.jit_addr[i]) )
			jmp_list.jit_addr[i] += diff;
}
*/

