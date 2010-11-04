
#include <sys/mman.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <readline/readline.h>

#include "debug.h"
#include "codeexec.h"

int hex2bin(char *hex, char *bin)
{
	int i=0, op=0, c, sz=0;

	while ( (c = *hex++) )
	{
		if (isxdigit(c))
		{
			if (c <= '9')
				op |= c-48;
			else
				op |= (c&7)+9;

			i++;

			if (i&1)
				op <<=4;
			else
			{
				bin[sz] = op;
				sz++;
				op = 0;
			}
		}
	}
	return sz;
}

void fill(char *bin, int size)
{
	int i;
	for (i=0; i<size; i++)
		bin[i]=(char)i;
}

int main(int argc, char **argv)
{
	debug_init(stdout);

	char *mem = get_rwmem(4096),
	     *mem_old = get_rwmem(4096),
	     *buf = get_rwmem(4096),
	     *fx_new = buf,
	     *fx_old = buf+512;

	long *regs_old = (long *)(buf + 1024),
	     *regs_new = (long *)(buf + 1060);

	fill(mem, 4096);

	save_fx(fx_new);
	regs_new[0] = 0xEAEAEAEA;
	regs_new[1] = 0xECECECEC;
	regs_new[2] = 0xEDEDEDED;
	regs_new[3] = (long)mem;
	regs_new[4] = (long)&mem[4096-sizeof(void*)];
	regs_new[5] = 0xBBBBBBBB;
	regs_new[6] = 0x51515151;
	regs_new[7] = 0xD1D1D1D1;
	regs_new[8] = 0x00000246;
	memcpy(regs_old, regs_new, 36);
	memcpy(fx_old, fx_new, 512);
	memcpy(mem_old, mem, 4096);
	char *input, input_sz;
	while ( (input = readline("shellcode>")) )
	{
		input_sz = hex2bin(input, input);
		load_fx(fx_new);
		codeexec(input, input_sz, regs_new);
		save_fx(fx_new);

		if (strchr(input, 'p'))
		{
			print_pushadfd(regs_old, regs_new);
			memcpy(regs_old, regs_new, 36);
		}

		if (strchr(input, 'x'))
		{
			print_fxsave(fx_old, fx_new);
			memcpy(fx_old, fx_new, 512);
		}

		if (strchr(input, 'm'))
		{
			printhex_diff(mem_old, 4096, mem, 4096, 1);
			memcpy(mem_old, mem, 4096);
		}

		free(input);
	}
	printf("\n");
	exit(EXIT_SUCCESS);
}

