
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


#include <sys/mman.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "codeexec.h"
#include "taint_code.h"
#include "opcodes.h"
#include "threads.h"
#include "error.h"

int fd_vprintf(int fd, const char *format, va_list ap)
{
    return vprintf(format, ap);
}

int fd_printf(int fd, const char *format, ...)
{
    int ret;
    va_list ap;
    va_start(ap, format);
    ret=fd_vprintf(fd, format, ap);
    va_end(ap);
    return ret;
}

void runtime_ijmp(void) { die("calling placeholder"); }
void jit_return(void) { die("calling placeholder"); }
void jit_fragment_exit(void) { die("calling placeholder"); }
void clear_jmp_cache(void) { die("calling placeholder"); }

char user_sigaction_list[1];

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
char opcode[256]; int oplen;

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
	0x00000246, /* flags */
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

char *mrm_tests[] = 
{
	"\x00",
	"\x01",
	"\x02",
	"\x03",
	"\x04\x24",
	"\x45\x00",
	"\x06",
	"\x07",
	"\xC0",
	"\xC1",
	"\xC2",
	"\xC3",
	"\xC4",
	"\xC5",
	"\xC6",
	"\xC7",
	NULL
};

char *mrm_cmpxchg8b[] = 
{
	"\x08",
	"\x09",
	"\x0a",
	"\x0b",
	"\x0c\x24",
	"\x4d\x00",
	"\x0e",
	"\x0f",
	NULL
};

int is_memop(char *mrm)
{
    return (mrm[0] & 0xC0) != 0xC0;
}

int mrm_len(char *mrm)
{
	int len = 1;
	if ( ((mrm[0]&0xC0) != 0xC0) && ((mrm[0]&0x07) == 0x04) ) len+=1;
	if ( ((mrm[0]&0xC7) == 0x04) && ((mrm[1]&0x07) == 0x05) ) len+=4;
	if   ((mrm[0]&0xC7) == 0x05)                              len+=4;
	if   ((mrm[0]&0xC0) == 0x40)                              len+=1;
	if   ((mrm[0]&0xC0) == 0x80)                              len+=4;
	return len;
}

char *do_lea(char *m)
{
	int mlen = mrm_len(m);
    char op[mlen+1];
    long regs[9];
	memcpy(regs, regs_test, 9*sizeof(long));

    op[0] = '\x8D';
    memcpy(&op[1], m, mlen);
	op[1] &= ~0x38;

    codeexec((char *)op, mlen+1, (long *)regs);
    return (char *)regs[0];
}

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
	int e=0;
	if (bcmp(regs_backup, regs_test, 36)) 
	{
		printhex_diff(regs_backup, 36, regs_test, 36, 4);
		e=err = EXIT_FAILURE;
	}
	if (bcmp(mem_backup, mem_test, 32)) 
	{
		printhex_diff(mem_backup, 32, mem_test, 32, 1);
		e=err = EXIT_FAILURE;
	}
	if (bcmp(taintmem_backup, taintmem_test, 32)) 
	{
		printhex_diff(taintmem_backup, 32, taintmem_test, 32, 1);
		e=err = EXIT_FAILURE;
	}
	if ( (bcmp(fx_backup, fx_test, 240)) || /* exempt scratch register */
	     (bcmp(&fx_backup[256], &fx_test[256], 256)) )
	{
		print_fxsave(fx_backup, fx_test);
		e=err = EXIT_FAILURE;
	}
	if (e)
{
		printhex(opcode, oplen);
exit(1);}
}

void ref_copy_reg32_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] = taint_regs[from_reg];
}

void ref_copy_reg16_to_reg16(int from_reg, int to_reg)
{
	taint_regs[to_reg] = (taint_regs[to_reg]&0xFFFF0000) | (taint_regs[from_reg]&0xFFFF);
}

char *get_byte_reg(int reg)
{
	int m[] = { 0, 4, 8, 12, 1, 5, 9, 13 };
	return &((char*)taint_regs)[m[reg]];
}

void ref_copy_reg8_to_reg8(int from_reg, int to_reg)
{
	char *f = get_byte_reg(from_reg),
	     *t = get_byte_reg(to_reg);

	*t = *f;
}

void ref_or_reg8_to_reg8(int from_reg, int to_reg)
{
	char *f = get_byte_reg(from_reg),
	     *t = get_byte_reg(to_reg);

	*t |= *f;
}

void ref_or_reg32_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] |= taint_regs[from_reg];
}

void ref_or_reg16_to_reg16(int from_reg, int to_reg)
{
	taint_regs[to_reg] |= taint_regs[from_reg]&0xFFFF;
}

void ref_erase_reg32(int reg)
{
	taint_regs[reg] = 0;
}

void ref_erase_reg16(int reg)
{
	taint_regs[reg] &= 0xFFFF0000;
}

void ref_erase_reg8(int reg)
{
	*get_byte_reg(reg) = 0;
}

void ref_erase_hireg16(int reg)
{
	taint_regs[reg] &= 0xFFFF;
}

void ref_erase_mem32(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_erase_reg32(mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) = 0;
}

void ref_erase_mem16(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_erase_reg16(mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) &= 0xFFFF0000;
}

void ref_erase_mem8(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_erase_reg8(mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(char *)(addr+off) = 0;
}

void ref_erase_push32(long off)
{
	*(long *)(regs_test[4]-4+off) = 0;;
}

void ref_erase_push16(long off)
{
	*(short *)(regs_test[4]-2+off) = 0;;
}

void ref_copy_mem32_to_reg32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg32_to_reg32(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = *(long *)(addr+off);
}

void ref_copy_mem32_to_eax(char *mrm, long off)
{
	int reg = 0;
	if (!is_memop(mrm)) { ref_copy_reg32_to_reg32(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = *(long *)(addr+off);
}

void ref_copy_mem16_to_reg16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg16_to_reg16(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = (taint_regs[reg]&0xFFFF0000) | (*(long *)(addr+off)&0xFFFF);
}

void ref_copy_mem16_to_ax(char *mrm, long off)
{
	int reg = 0;
	if (!is_memop(mrm)) { ref_copy_reg16_to_reg16(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = (taint_regs[reg]&0xFFFF0000) | (*(long *)(addr+off)&0xFFFF);
}

void ref_copy_mem8_to_reg8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg8_to_reg8(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);
	char *t = get_byte_reg(reg);

	*t = *(char *)(addr+off);
}

void ref_copy_mem8_to_al(char *mrm, long off)
{
	int reg = 0;
	if (!is_memop(mrm)) { ref_copy_reg8_to_reg8(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);
	char *t = get_byte_reg(reg);

	*t = *(char *)(addr+off);
}

void ref_or_mem8_to_reg8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg8_to_reg8(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);
	char *t = get_byte_reg(reg);

	*t |= *(char *)(addr+off);
}

void ref_copy_reg32_to_mem32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg32_to_reg32(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) = taint_regs[reg];
}

void ref_or_reg32_to_mem32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg32_to_reg32(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) |= taint_regs[reg];
}

void ref_copy_reg16_to_mem16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg16_to_reg16(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) = (*(long *)(addr+off)&0xFFFF0000) | (taint_regs[reg]&0xFFFF);
}

void ref_copy_reg8_to_mem8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg8_to_reg8(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);
	char *f = get_byte_reg(reg);

	*(char *)(addr+off) = *f;
}

void ref_copy_push_reg32(int reg, long off)
{
	*(long *)(regs_test[4]+off-4) = taint_regs[reg];
}

void ref_copy_push_reg16(int reg, long off)
{
	*(short *)(regs_test[4]+off-2) = taint_regs[reg];
}

void ref_copy_pop_reg32(int reg, long off)
{
	taint_regs[reg] = *(long *)(regs_test[4]+off);
}

void ref_copy_pop_reg16(int reg, long off)
{
	taint_regs[reg] = (taint_regs[reg]&0xFFFF0000) | *(unsigned short *)(regs_test[4]+off);
}

void ref_copy_push_mem32(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_copy_push_reg32(mrm[0]&0x7, off); return; }
	char *addr = do_lea(mrm);
	*(long *)(regs_test[4]+off-4) = *(long *)(addr+off);
}

void ref_copy_push_mem16(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_copy_push_reg16(mrm[0]&0x7, off); return; }
	char *addr = do_lea(mrm);

	*(short *)(regs_test[4]+off-2) = *(short *)(addr+off);
}

void ref_copy_pop_mem32(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_copy_pop_reg32(mrm[0]&0x7, off); return; }
	char *addr = do_lea(mrm);

	*(long *)(addr+off) = *(long *)(regs_test[4]+off);
}

void ref_copy_pop_mem16(char *mrm, long off)
{
	if (!is_memop(mrm)) { ref_copy_pop_reg16(mrm[0]&0x7, off); return; }
	char *addr = do_lea(mrm);

	*(short *)(addr+off) = *(short *)(regs_test[4]+off);
}

void ref_copy_eax_to_addr32(long addr, long off)
{
	*(long*)(addr+off) = taint_regs[0];
}

void ref_copy_ax_to_addr16(long addr, long off)
{
	*(short*)(addr+off) = taint_regs[0];
}

void ref_copy_al_to_addr8(long addr, long off)
{
	*(char*)(addr+off) = taint_regs[0];
}

void ref_copy_addr32_to_eax(long addr, long off)
{
	taint_regs[0] = *(long*)(addr+off);
}

void ref_copy_addr16_to_ax(long addr, long off)
{
	taint_regs[0] = (taint_regs[0]&0xFFFF0000) | *(unsigned short*)(addr+off);
}

void ref_copy_addr8_to_al(long addr, long off)
{
	taint_regs[0] = (taint_regs[0]&0xFFFFFF00) | *(unsigned char*)(addr+off);
}

void ref_copy_eax_to_str32(long off)
{
	*(long*)(off+regs_test[7]) = taint_regs[0];
}

void ref_copy_ax_to_str16(long off)
{
	*(short*)(off+regs_test[7]) = (short)taint_regs[0];
}

void ref_copy_al_to_str8(long off)
{
	*(char*)(off+regs_test[7]) = (char)taint_regs[0];
}

void ref_copy_str32_to_eax(long off)
{
	taint_regs[0] = *(long*)(off+regs_test[6]);
}

void ref_copy_str16_to_ax(long off)
{
	taint_regs[0] = (taint_regs[0]&0xFFFF0000) | *(unsigned short*)(off+regs_test[6]);
}

void ref_copy_str8_to_al(long off)
{
	taint_regs[0] = (taint_regs[0]&0xFFFFFF00) | *(unsigned char*)(off+regs_test[6]);
}

void ref_copy_str32_to_str32(long off)
{
	*(long*)(off+regs_test[7]) = *(long*)(off+regs_test[6]);
}

void ref_copy_str16_to_str16(long off)
{
	*(short*)(off+regs_test[7]) = *(short*)(off+regs_test[6]);
}

void ref_copy_str8_to_str8(long off)
{
	*(char*)(off+regs_test[7]) = *(char*)(off+regs_test[6]);
}

void ref_or_mem32_to_reg32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg32_to_reg32(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] |= *(long *)(addr+off);
}

void ref_or_mem16_to_reg16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg16_to_reg16(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] |= *(unsigned short *)(addr+off);
}

void ref_xor_mem32_to_reg32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg32(reg); return; }
	ref_or_mem32_to_reg32(mrm, off);
}

void ref_xor_mem16_to_reg16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg16(reg); return; }
	ref_or_mem16_to_reg16(mrm, off);
}

void ref_xor_mem8_to_reg8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg8(reg); return; }
	ref_or_mem8_to_reg8(mrm, off);
}

void ref_or_reg8_to_mem8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg8_to_reg8(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(char *)(addr+off) |= *get_byte_reg(reg);
}

void ref_or_reg16_to_mem16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_or_reg16_to_reg16(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	*(short *)(addr+off) |= taint_regs[reg];
}

void ref_xor_reg32_to_mem32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg32(reg); return; }
	ref_or_reg32_to_mem32(mrm, off);
}

void ref_xor_reg16_to_mem16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg16(reg); return; }
	ref_or_reg16_to_mem16(mrm, off);
}

void ref_xor_reg8_to_mem8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm) && reg == (mrm[0]&0x7)) { ref_erase_reg8(reg); return; }
	ref_or_reg8_to_mem8(mrm, off);
}


void ref_swap_reg32_reg32(int reg1, int reg2)
{
	long tmp = taint_regs[reg1];
	taint_regs[reg1] = taint_regs[reg2];
	taint_regs[reg2] = tmp;
}

void ref_swap_reg16_reg16(int reg1, int reg2)
{
	unsigned short tmp = (unsigned short)taint_regs[reg1];
	taint_regs[reg1] = (taint_regs[reg1]&0xFFFF0000) | (taint_regs[reg2]&0xFFFF);
	taint_regs[reg2] = (taint_regs[reg2]&0xFFFF0000) | tmp;
}

void ref_swap_reg8_reg8(int reg1, int reg2)
{
	char *r1 = get_byte_reg(reg1);
	char *r2 = get_byte_reg(reg2);
	char tmp = *r1; *r1=*r2; *r2=tmp;
}

void ref_swap_reg32_mem32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_swap_reg32_reg32(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	long tmp = taint_regs[reg];
	taint_regs[reg] = *(long *)(addr+off);
	*(long *)(addr+off) = tmp;
}

void ref_swap_reg16_mem16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_swap_reg16_reg16(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	unsigned short tmp = *(unsigned short*)(addr+off);
	*(unsigned short*)(addr+off) = taint_regs[reg]&0xFFFF;
	taint_regs[reg] = (taint_regs[reg]&0xFFFF0000) | tmp;
}

void ref_swap_reg8_mem8(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_swap_reg8_reg8(reg, mrm[0]&0x7); return; }
	char *addr = do_lea(mrm);

	char *r = get_byte_reg(reg);

	char tmp = *(char*)(addr+off);
	*(char*)(addr+off) = *r;
	*r = tmp;
}

void ref_copy_reg16_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] = taint_regs[from_reg]&0xFFFF;
}

void ref_copy_reg8_to_reg32(int from_reg, int to_reg)
{
	taint_regs[to_reg] = taint_regs[from_reg]&0xFF;
}

void ref_copy_reg8_to_reg16(int from_reg, int to_reg)
{
	char *f=get_byte_reg(from_reg);
	taint_regs[to_reg] = (taint_regs[to_reg]&0xFFFF0000) | (unsigned char)(*f);
}

void ref_copy_mem16_to_reg32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg16_to_reg32(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = *(unsigned short*)(addr+off);
}

void ref_copy_mem8_to_reg32(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg8_to_reg32(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = *(unsigned char*)(addr+off);
}

void ref_copy_mem8_to_reg16(char *mrm, long off)
{
	int reg = (mrm[0]>>3)&0x7;
	if (!is_memop(mrm)) { ref_copy_reg8_to_reg16(mrm[0]&0x7, reg); return; }
	char *addr = do_lea(mrm);

	taint_regs[reg] = (taint_regs[reg]&0xFFFF0000) | *(unsigned char*)(addr+off);
}

void ref_erase_eax_edx(void)
{
	taint_regs[0] = taint_regs[2] = 0;
}

void ref_erase_ax_dx(void)
{
	taint_regs[0] &= 0xFFFF0000;
	taint_regs[2] &= 0xFFFF0000;
}

void ref_erase_eax(void)
{
	taint_regs[0] = 0;
}

void ref_erase_ax(void)
{
	taint_regs[0] &= 0xFFFF0000;
}

void ref_erase_al(void)
{
	taint_regs[0] &= 0xFFFFFF00;
}

void ref_leave32(long off)
{
	taint_regs[4] = taint_regs[5];
	taint_regs[5] = *(long*)(regs_test[5]+off);
}

void ref_leave16(long off)
{
	taint_regs[4] = (taint_regs[4]&0xffff0000) | (taint_regs[5]&0xffff);
	taint_regs[5] = (taint_regs[5]&0xffff0000) | *(short*)(regs_test[5]+off);
}

void ref_copy_popa32(long off)
{
	long tmp=regs_test[4];
	ref_copy_pop_reg32(7, off); regs_test[4]+=4;
	ref_copy_pop_reg32(6, off); regs_test[4]+=4;
	ref_copy_pop_reg32(5, off); regs_test[4]+=4;
	                            regs_test[4]+=4;
	ref_copy_pop_reg32(3, off); regs_test[4]+=4;
	ref_copy_pop_reg32(2, off); regs_test[4]+=4;
	ref_copy_pop_reg32(1, off); regs_test[4]+=4;
	ref_copy_pop_reg32(0, off); regs_test[4]+=4;
	regs_test[4]=tmp;
}

void ref_copy_popa16(long off)
{
	long tmp=regs_test[4];
	ref_copy_pop_reg16(7, off); regs_test[4]+=2;
	ref_copy_pop_reg16(6, off); regs_test[4]+=2;
	ref_copy_pop_reg16(5, off); regs_test[4]+=2;
	ref_copy_pop_reg16(4, off); regs_test[4]+=2;
	ref_copy_pop_reg16(3, off); regs_test[4]+=2;
	ref_copy_pop_reg16(2, off); regs_test[4]+=2;
	ref_copy_pop_reg16(1, off); regs_test[4]+=2;
	ref_copy_pop_reg16(0, off); regs_test[4]+=2;
	regs_test[4]=tmp;
}

void ref_copy_pusha32(long off)
{
	long tmp=regs_test[4];
	ref_copy_push_reg32(0, off); regs_test[4]-=4;
	ref_copy_push_reg32(1, off); regs_test[4]-=4;
	ref_copy_push_reg32(2, off); regs_test[4]-=4;
	ref_copy_push_reg32(3, off); regs_test[4]-=4;
	ref_copy_push_reg32(4, off); regs_test[4]-=4;
	ref_copy_push_reg32(5, off); regs_test[4]-=4;
	ref_copy_push_reg32(6, off); regs_test[4]-=4;
	ref_copy_push_reg32(7, off); regs_test[4]-=4;
	regs_test[4]=tmp;
}

void ref_copy_pusha16(long off)
{
	long tmp=regs_test[4];
	ref_copy_push_reg16(0, off); regs_test[4]-=2;
	ref_copy_push_reg16(1, off); regs_test[4]-=2;
	ref_copy_push_reg16(2, off); regs_test[4]-=2;
	ref_copy_push_reg16(3, off); regs_test[4]-=2;
	ref_copy_push_reg16(4, off); regs_test[4]-=2;
	ref_copy_push_reg16(5, off); regs_test[4]-=2;
	ref_copy_push_reg16(6, off); regs_test[4]-=2;
	ref_copy_push_reg16(7, off); regs_test[4]-=2;
	regs_test[4]=tmp;
}


void ref_cmpxchg8(int flags, char *mrm, long off)
{
	if (flags & 0x40)
		ref_copy_reg8_to_mem8(mrm, off);
	else
		ref_copy_mem8_to_al(mrm, off);
}

void ref_cmpxchg16(int flags, char *mrm, long off)
{
	if (flags & 0x40)
		ref_copy_reg16_to_mem16(mrm, off);
	else
		ref_copy_mem16_to_ax(mrm, off);
}

void ref_cmpxchg32(int flags, char *mrm, long off)
{
	if (flags & 0x40)
		ref_copy_reg32_to_mem32(mrm, off);
	else
		ref_copy_mem32_to_eax(mrm, off);
}

void ref_cmpxchg8b(int flags, char *mrm, long off)
{
	if (!is_memop(mrm))
		return;

	char *addr = do_lea(mrm);
	if (flags & 0x40)
	{
		*(long*)(addr+off)   = taint_regs[3];
		*(long*)(addr+off+4) = taint_regs[1];
	}
	else
	{
		taint_regs[0] = *(long*)(addr+off);
		taint_regs[2] = *(long*)(addr+off+4);
	}
}

void test_cmpxchg(int (*taint_pre)(char *, char *, long),
                  int (*taint_post)(char *, char *, long),
                  void (*ref_op)(int, char *, long))
{
	int i, j, f;
	char mrm[16];

	for (f=0x200; f<0x300; f++) if ( (f & 0x28) == 0 && (f & 0x2) )
		for (i=0; i<8; i++)
			for (j=0; mrm_tests[j]; j++)
			{
				memcpy(mrm, mrm_tests[j], mrm_len(mrm_tests[j]));
				mrm[0] |= i<<3;
				setup();
				regs_test[8] = f;
				ref_op(f, mrm, offset);
				backup();
				regs_test[8] = f;
				load_fx(fx_test);
				oplen = taint_pre(opcode, mrm, offset);
				oplen += taint_post(&opcode[oplen], mrm, offset);
				codeexec((char *)opcode, oplen, (long *)regs_test);
				save_fx(fx_test);
				diff();
			}
}

void test_mem(int (*taint_op)(char *, char *, long), void (*ref_op)(char *, long))
{
	int i, j;
	char mrm[16];

	for (i=0; i<8; i++)
		for (j=0; mrm_tests[j]; j++)
		{
			memcpy(mrm, mrm_tests[j], mrm_len(mrm_tests[j]));
			mrm[0] |= i<<3;
			setup();
			ref_op(mrm, offset);
			backup();
			load_fx(fx_test);
			oplen = taint_op(opcode, mrm, offset);
			codeexec((char *)opcode, oplen, (long *)regs_test);
			save_fx(fx_test);
			diff();
		}
}

void test_reg(int (*taint_op)(char *, int), void (*ref_op)(int))
{
	int i;

	for (i=0; i<8; i++)
	{
		setup();
		ref_op(i);
		backup();
		load_fx(fx_test);
		oplen = taint_op(opcode, i);
		codeexec((char *)opcode, oplen, (long *)regs_test);
		save_fx(fx_test);
		diff();
	}
}

void test_stackop(int (*taint_op)(char *, int, long), void (*ref_op)(int, long))
{
	int i;

	for (i=0; i<8; i++)
	{
		setup();
		ref_op(i, offset);
		backup();
		load_fx(fx_test);
		oplen = taint_op(opcode, i, offset);
		codeexec((char *)opcode, oplen, (long *)regs_test);
		save_fx(fx_test);
		diff();
	}
}

void test_addr(int (*taint_op)(char *, long, long), void (*ref_op)(long, long))
{
	int i;

	for (i=0; i<8; i++)
	{
		setup();
		ref_op(regs_test[i], offset);
		backup();
		load_fx(fx_test);
		oplen = taint_op(opcode, regs_test[i], offset);
		codeexec((char *)opcode, oplen, (long *)regs_test);
		save_fx(fx_test);
		diff();
	}
}

void test_impl(int (*taint_op)(char *, long), void (*ref_op)(long))
{
	setup();
	ref_op(offset);
	backup();
	load_fx(fx_test);
	oplen = taint_op(opcode, offset);
	codeexec((char *)opcode, oplen, (long *)regs_test);
	save_fx(fx_test);
	diff();
}

void test_impl2(int (*taint_op)(char *), void (*ref_op)(void))
{
	setup();
	ref_op();
	backup();
	load_fx(fx_test);
	oplen = taint_op(opcode);
	codeexec((char *)opcode, oplen, (long *)regs_test);
	save_fx(fx_test);
	diff();
}

void test_pushpop(int (*taint_op)(char *, long), void (*ref_op)(long), long sp)
{
	setup();
	regs_test[4] = sp;
	ref_op(offset);
	backup();
	regs_test[4] = sp;
	load_fx(fx_test);
	oplen = taint_op(opcode, offset);
	codeexec((char *)opcode, oplen, (long *)regs_test);
	save_fx(fx_test);
	diff();
}

void test_reg2(int (*taint_op)(char *, int, int), void (*ref_op)(int, int))
{
	int i, j;

	for (i=0; i<8; i++)
		for (j=0; j<8; j++)
		{
			setup();
			ref_op(i, j);
			backup();
			load_fx(fx_test);
			oplen = taint_op(opcode, i, j);
			codeexec((char *)opcode, oplen, (long *)regs_test);
			save_fx(fx_test);
			diff();
		}
}


static const long regs_lea[9] =
{
	0x00000001,
	0x00000010,
	0x00000100,
	0x00001000,
	0x00010000,
	0x00100000,
	0x01000000,
	0x10000000,
	0x00000246, /* flags */
};

static const long taint_regs_lea[8] =
{
	0x0000000f,
	0x000000f0,
	0x00000f00,
	0x0000f000,
	0x000f0000,
	0x00f00000,
	0x0f000000,
	0xf0000000,
};

typedef void (*mrm_test_t)(char *, int);

void test_lea(char *mrm, int mem_len)
{
	setup();
	memcpy(regs_test, regs_lea, 36);
	memcpy(taint_regs, taint_regs_lea, 32);
	unsigned long ref = (unsigned long)do_lea(mrm);
	int dest = (mrm[0]>>3)&7;
	int i;
	taint_regs[dest] = 0;
	for (i=0; i<32; i+=4)
		if (ref & (0xf<<i))
			taint_regs[dest] |= (0xf<<i);

	memcpy(regs_test, regs_lea, 36);
	backup();
	memcpy(regs_test, regs_lea, 36);
	memcpy(taint_regs, taint_regs_lea, 32);
	load_fx(fx_test);
	oplen = taint_lea(opcode, mrm, offset);
	codeexec((char *)opcode, oplen, (long *)regs_test);
	save_fx(fx_test);
	diff();
}

void mrm_generator(mrm_test_t cb)
{
	unsigned char m[16];
	int mrm, sib;

	long disp32 = 0x00000000;
	char disp8  = 0x00;

	for (mrm=0; mrm<0x100; mrm++)
	{
		if ( (mrm & 0xC0) == 0xC0 ) /* RM is a register */
			continue;

		m[0] = mrm;

		if ( (mrm & 0x7) == 0x04 )
			for (sib=0; sib<0x100; sib++)
			{
				m[1] = sib;
				if ( (mrm & 0xC0) == 0x40 )
				{
					m[2] = disp8;
					cb((char*)m, 3);
				}
				else if ( ( (mrm & 0xC0) == 0x80 ) ||
				          ( (sib & 0x07) == 0x05 ) )
				{
					imm_to((char*)&m[2], disp32);
					cb((char*)m, 6);
				}
				else
					cb((char*)m, 2);
			}
		else
		{
			if ( (mrm & 0xC0) == 0x40 )
			{
				m[1] = disp8;
				cb((char*)m, 2);
			}
			else if ( ( (mrm & 0xC0) == 0x80 ) ||
			          ( (mrm & 0xC7) == 0x05 ) )
			{
				imm_to((char *)&m[1], disp32);
				cb((char*)m, 5);
			}
			else
				cb((char*)m, 1);

		}
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

	init_threads();

	test_reg2(taint_copy_reg32_to_reg32, ref_copy_reg32_to_reg32);
	test_reg2(taint_copy_reg16_to_reg16, ref_copy_reg16_to_reg16);
	test_reg2(taint_copy_reg8_to_reg8, ref_copy_reg8_to_reg8);

	test_mem(taint_copy_mem32_to_reg32, ref_copy_mem32_to_reg32);
	test_mem(taint_copy_mem16_to_reg16, ref_copy_mem16_to_reg16);
	test_mem(taint_copy_mem8_to_reg8, ref_copy_mem8_to_reg8);

	test_mem(taint_copy_reg32_to_mem32, ref_copy_reg32_to_mem32);
	test_mem(taint_copy_reg16_to_mem16, ref_copy_reg16_to_mem16);
	test_mem(taint_copy_reg8_to_mem8, ref_copy_reg8_to_mem8);

	test_stackop(taint_copy_push_reg32, ref_copy_push_reg32);
	test_stackop(taint_copy_push_reg16, ref_copy_push_reg16);

	test_mem(taint_copy_push_mem32, ref_copy_push_mem32);
	test_mem(taint_copy_push_mem16, ref_copy_push_mem16);

	test_stackop(taint_copy_pop_reg32, ref_copy_pop_reg32);
	test_stackop(taint_copy_pop_reg16, ref_copy_pop_reg16);

	test_mem(taint_copy_pop_mem32, ref_copy_pop_mem32);
	test_mem(taint_copy_pop_mem16, ref_copy_pop_mem16);

	test_addr(taint_copy_eax_to_addr32, ref_copy_eax_to_addr32);
	test_addr(taint_copy_ax_to_addr16, ref_copy_ax_to_addr16);
	test_addr(taint_copy_al_to_addr8, ref_copy_al_to_addr8);

	test_addr(taint_copy_addr32_to_eax, ref_copy_addr32_to_eax);
	test_addr(taint_copy_addr16_to_ax, ref_copy_addr16_to_ax);
	test_addr(taint_copy_addr8_to_al, ref_copy_addr8_to_al);

	test_impl(taint_copy_eax_to_str32, ref_copy_eax_to_str32);
	test_impl(taint_copy_ax_to_str16, ref_copy_ax_to_str16);
	test_impl(taint_copy_al_to_str8, ref_copy_al_to_str8);

	test_impl(taint_copy_str32_to_eax, ref_copy_str32_to_eax);
	test_impl(taint_copy_str16_to_ax, ref_copy_str16_to_ax);
	test_impl(taint_copy_str8_to_al, ref_copy_str8_to_al);

	test_impl(taint_copy_str32_to_str32, ref_copy_str32_to_str32);
	test_impl(taint_copy_str16_to_str16, ref_copy_str16_to_str16);
	test_impl(taint_copy_str8_to_str8, ref_copy_str8_to_str8);

	test_reg(taint_erase_reg32, ref_erase_reg32);
	test_reg(taint_erase_reg16, ref_erase_reg16);
	test_reg(taint_erase_reg8, ref_erase_reg8);

	test_mem(taint_erase_mem32, ref_erase_mem32);
	test_mem(taint_erase_mem16, ref_erase_mem16);
	test_mem(taint_erase_mem8, ref_erase_mem8);

	test_reg(taint_erase_hireg16, ref_erase_hireg16);

	test_impl(taint_erase_push32, ref_erase_push32);
	test_impl(taint_erase_push16, ref_erase_push16);

	test_reg2(taint_or_reg32_to_reg32, ref_or_reg32_to_reg32);
	test_reg2(taint_or_reg16_to_reg16, ref_or_reg16_to_reg16);
	test_reg2(taint_or_reg8_to_reg8, ref_or_reg8_to_reg8);

	test_mem(taint_or_reg32_to_mem32, ref_or_reg32_to_mem32);
	test_mem(taint_or_reg16_to_mem16, ref_or_reg16_to_mem16);
	test_mem(taint_or_reg8_to_mem8, ref_or_reg8_to_mem8);

	test_mem(taint_or_mem32_to_reg32, ref_or_mem32_to_reg32);
	test_mem(taint_or_mem16_to_reg16, ref_or_mem16_to_reg16);
	test_mem(taint_or_mem8_to_reg8, ref_or_mem8_to_reg8);

	test_mem(taint_xor_reg32_to_mem32, ref_xor_reg32_to_mem32);
	test_mem(taint_xor_reg16_to_mem16, ref_xor_reg16_to_mem16);
	test_mem(taint_xor_reg8_to_mem8, ref_xor_reg8_to_mem8);

	test_mem(taint_xor_mem32_to_reg32, ref_xor_mem32_to_reg32);
	test_mem(taint_xor_mem16_to_reg16, ref_xor_mem16_to_reg16);
	test_mem(taint_xor_mem8_to_reg8, ref_xor_mem8_to_reg8);

	test_reg2(taint_swap_reg32_reg32, ref_swap_reg32_reg32);
	test_reg2(taint_swap_reg16_reg16, ref_swap_reg16_reg16);
	test_reg2(taint_swap_reg8_reg8, ref_swap_reg8_reg8);

	test_mem(taint_swap_reg32_mem32, ref_swap_reg32_mem32);
	test_mem(taint_swap_reg16_mem16, ref_swap_reg16_mem16);
	test_mem(taint_swap_reg8_mem8, ref_swap_reg8_mem8);

	test_reg2(taint_copy_reg16_to_reg32, ref_copy_reg16_to_reg32);
	test_reg2(taint_copy_reg8_to_reg32, ref_copy_reg8_to_reg32);
	test_reg2(taint_copy_reg8_to_reg16, ref_copy_reg8_to_reg16);

	test_mem(taint_copy_mem16_to_reg32, ref_copy_mem16_to_reg32);
	test_mem(taint_copy_mem8_to_reg32, ref_copy_mem8_to_reg32);
	test_mem(taint_copy_mem8_to_reg16, ref_copy_mem8_to_reg16);

	test_impl2(taint_erase_eax_edx, ref_erase_eax_edx);
	test_impl2(taint_erase_ax_dx, ref_erase_ax_dx);
	test_impl2(taint_erase_eax, ref_erase_eax);
	test_impl2(taint_erase_ax, ref_erase_ax);
	test_impl2(taint_erase_al, ref_erase_al);

	test_pushpop(taint_copy_popa32, ref_copy_popa32, (long)mem_test);
	test_pushpop(taint_copy_popa16, ref_copy_popa16, (long)mem_test);

	test_pushpop(taint_copy_pusha32, ref_copy_pusha32, 32+(long)mem_test);
	test_pushpop(taint_copy_pusha16, ref_copy_pusha16, 32+(long)mem_test);

	test_impl(taint_leave32, ref_leave32);
	test_impl(taint_leave16, ref_leave16);

	test_cmpxchg(taint_cmpxchg8_pre, taint_cmpxchg8_post, ref_cmpxchg8);
	test_cmpxchg(taint_cmpxchg16_pre, taint_cmpxchg16_post, ref_cmpxchg16);
	test_cmpxchg(taint_cmpxchg32_pre, taint_cmpxchg32_post, ref_cmpxchg32);
	test_cmpxchg(taint_cmpxchg8b_pre, taint_cmpxchg8b_post, ref_cmpxchg8b);

	mrm_generator(test_lea);

	exit(err);
}
