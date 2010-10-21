
#include <sys/mman.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "codeexec.h"
#include "taint.h"
#include "opcodes.h"

long imm_at(char *addr, long size)
{
	long imm=0;
	memcpy(&imm, addr, size);
	if (size == 1)
		return *(signed char*)&imm;
	else
		return imm;
}

void imm_to(char *dest, long imm)
{
	memcpy(dest, &imm, sizeof(long));
}

void die()
{

}

void fill(char *bin, int size)
{
	int i;
	for (i=0; i<size; i++)
		bin[i]=(char)i;
}

long mem_test[8], mem_backup[8], taintmem_test[8], taintmem_backup[8];
char fx_test[512]   __attribute__ ((aligned (16))),
     fx_backup[512] __attribute__ ((aligned (16))),
     fx_orig[512]   __attribute__ ((aligned (16)));
long regs_test[9], regs_backup[9];
long offset;
int err = EXIT_SUCCESS;
long *taint_regs = ((long *)&fx_test[256]);

long regs_orig[] =
{
	(long)mem_test,
	(long)mem_test+4,
	(long)mem_test+8,
	(long)mem_test+12,
	(long)mem_test+16,
	(long)mem_test+20,
	(long)mem_test+24,
	(long)mem_test+28,
	0x09000246, /* flags */
};

long scratch_orig[] =
{
	0x15796276,
	0x97840632,
	0x85498748,
	0x45745459,
};

long taint_orig[] =
{
	0x01020408,
	0x02040810,
	0x04081020,
	0x08102040,
	0x10204080,
	0x20408001,
	0x40800102,
	0x80010204,
};

long taintmem_orig[] =
{
	0x00001111,
	0x00110011,
	0x11110000,
	0x01010101,
	0x11001100,
	0x11000011,
	0x10101010,
	0x00111100,
};

char *reg_addr_mrm[] = 
{
	"\x00",
	"\x01",
	"\x02",
	"\x03",
	"\x04\x24",
	"\x45\x00",
	"\x06",
	"\x07",
};

void setup(void)
{
	fill((char*)mem_test, 32);
	memcpy(taintmem_test, taintmem_orig, 32);
	memcpy(fx_test, fx_orig, 512);
	memcpy(regs_test, regs_orig, 9*sizeof(long));
}

void backup(void)
{
	memcpy(mem_backup, mem_test, 32);
	memcpy(taintmem_backup, taintmem_test, 32);
	memcpy(fx_backup, fx_test, 512);
	memcpy(regs_backup, regs_test, 9*sizeof(long));
	setup();
}

void diff(void)
{
	if (bcmp(regs_backup, regs_test, 36)) 
	{
		printhex_diff(regs_backup, 36, regs_test, 36, 4);
		err = EXIT_FAILURE;
	}
	if (bcmp(mem_backup, mem_test, 32)) 
	{
		printhex_diff(mem_backup, 32, mem_test, 32, 1);
		err = EXIT_FAILURE;
	}
	if (bcmp(taintmem_backup, taintmem_test, 32)) 
	{
		printhex_diff(taintmem_backup, 32, taintmem_test, 32, 1);
		err = EXIT_FAILURE;
	}
	if ( (bcmp(fx_backup, fx_test, 240)) || /* exempt scratch register */
	     (bcmp(&fx_backup[256], &fx_test[256], 256)) )
	{
		print_fxsave(fx_backup, fx_test);
		err = EXIT_FAILURE;
	}
}

void ref_copy_reg32_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] = taint_regs[from_reg];
}

void ref_combine_reg32_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] |= taint_regs[from_reg];
}

void ref_clear_reg32(int reg)
{
	taint_regs[reg] = 0;
}

void ref_clear_mem32(int reg, char *addr, long off)
{
	*(long *)(addr+off) = 0;
}

void ref_copy_mem32_to_reg32(int reg, char *addr, long off)
{
	taint_regs[reg] = *(long *)(addr+off);
}

void ref_copy_reg32_to_mem32(int reg, char *addr, long off)
{
	*(long *)(addr+off) = taint_regs[reg];
}

void ref_combine_mem32_to_reg32(int reg, char *addr, long off)
{
	taint_regs[reg] |= *(long *)(addr+off);
}

void ref_combine_reg32_to_mem32(int reg, char *addr, long off)
{
	*(long *)(addr+off) |= taint_regs[reg];
}

void test_mem(int (*taint_op)(char *, char *, long), void (*ref_op)(int, char *, long))
{
	int i, j;
	char opcode[64], mrm[2];

	for (i=0; i<8; i++)
		for (j=0; j<8; j++)
		{
			mrm[0] = reg_addr_mrm[j][0] | i<<3;
			mrm[1] = reg_addr_mrm[j][1];
			setup();
			ref_op(i, (char *)regs_test[j], offset);
			backup();
			load_fx(fx_test);
			int oplen = taint_op(opcode, mrm, offset);
			codeexec((char *)opcode, oplen, (long *)regs_test);
			save_fx(fx_test);
			diff();
		}
}

void test_reg(int (*taint_op)(char *, int), void (*ref_op)(int))
{
	int i;
	char opcode[64];

	for (i=0; i<8; i++)
	{
		setup();
		ref_op(i);
		backup();
		load_fx(fx_test);
		int oplen = taint_op(opcode, i);
		codeexec((char *)opcode, oplen, (long *)regs_test);
		save_fx(fx_test);
		diff();
	}
}

void test_reg2(int (*taint_op)(char *, int, int), void (*ref_op)(int, int))
{
	int i, j;
	char opcode[64];

	for (i=0; i<8; i++)
		for (j=0; j<8; j++)
		{
			setup();
			ref_op(i, j);
			backup();
			load_fx(fx_test);
			int oplen = taint_op(opcode, i, j);
			codeexec((char *)opcode, oplen, (long *)regs_test);
			save_fx(fx_test);
			diff();
		}
}

int main(int argc, char **argv)
{
	debug_init(stdout);
	save_fx(fx_orig);
	memcpy(&fx_orig[240], scratch_orig, 4*sizeof(long));
	memcpy(&fx_orig[256], taint_orig, 8*sizeof(long));
	offset = (long)taintmem_test - (long)mem_test;
	codeexec(NULL, 0, (long *)regs_orig);

	test_reg(taint_clear_reg32, ref_clear_reg32);
	test_reg2(taint_copy_reg32_to_reg32, ref_copy_reg32_to_reg32);
	test_reg2(taint_combine_reg32_to_reg32, ref_combine_reg32_to_reg32);
	test_mem(taint_clear_mem32, ref_clear_mem32);
	test_mem(taint_copy_mem32_to_reg32, ref_copy_mem32_to_reg32);
	test_mem(taint_copy_reg32_to_mem32, ref_copy_reg32_to_mem32);
	test_mem(taint_combine_reg32_to_mem32, ref_combine_reg32_to_mem32);
	test_mem(taint_combine_mem32_to_reg32, ref_combine_mem32_to_reg32);

	exit(err);
}
