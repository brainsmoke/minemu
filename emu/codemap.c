
#include "lib.h"
#include "mm.h"

#include "jit.h"
#include "error.h"
#include "jmp_cache.h"
#include "codemap.h"
#include "runtime.h"

#define MAX_CODEMAPS (128)
static code_map_t codemaps[MAX_CODEMAPS];
static unsigned n_codemaps = 0;

static void clear_code_map(int i)
{
	clear_jmp_mappings(codemaps[i].addr, codemaps[i].len);
	jit_free(codemaps[i].jit_addr);
	/* */
}

static void del_code_map(int i)
{
	clear_code_map(i);

	for (; i<n_codemaps; i++)
		codemaps[i] = codemaps[i+1];

	n_codemaps--;
}

code_map_t *find_code_map(char *addr)
{
	int i;

	for (i=0; i<n_codemaps; i++)
		if (contains(codemaps[i].addr, codemaps[i].len, addr))
			return &codemaps[i];

	return NULL;
}

code_map_t *find_jit_code_map(char *jit_addr)
{
	int i;

	for (i=0; i<n_codemaps; i++)
		if (contains(codemaps[i].jit_addr, codemaps[i].jit_len, jit_addr))
			return &codemaps[i];

	return NULL;
}

void add_code_region(char *addr, unsigned long len)
{
	int i;

	del_code_region(addr, len);

	if (n_codemaps >= MAX_CODEMAPS)
		die("Too many codemaps");

	for (i=n_codemaps; i>0; i--)
		if ( (unsigned long)addr < (unsigned long)codemaps[i-1].addr )
			codemaps[i] = codemaps[i-1];
		else
			break;

	codemaps[i] = (code_map_t){ addr, len, NULL, 0 };
	n_codemaps++;
}

void del_code_region(char *addr, unsigned long len)
{
	int i = n_codemaps-1;

	while (i >= 0)
	{
		if (!overlap(addr, len, codemaps[i].addr, codemaps[i].len))
		{
			i--;
			continue;
		}

		unsigned long start = (unsigned long)addr,
		              end = start + len,
		              o_start = (unsigned long)codemaps[i].addr,
		              o_end = o_start + codemaps[i].len;

		del_code_map(i);

		if ( start > o_start )
			add_code_region((char *)o_start, start-o_start);

		if ( o_end > end )
			add_code_region((char *)end, o_end-end);

		i = n_codemaps-1;
	}
}

