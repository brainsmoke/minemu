
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

typedef void (*func_t)(void);

#ifdef __i386__

static char *swap_regs =
	"\x87\x05XXXX"           /* xchg   %eax,regs+0x00 */
	"\x87\x0dXXXX"           /* xchg   %ecx,regs+0x04 */
	"\x87\x15XXXX"           /* xchg   %edx,regs+0x08 */
	"\x87\x1dXXXX"           /* xchg   %ebx,regs+0x0C */
	"\x87\x25XXXX"           /* xchg   %esp,regs+0x10 */
	"\x87\x2dXXXX"           /* xchg   %ebp,regs+0x14 */
	"\x87\x35XXXX"           /* xchg   %esi,regs+0x18 */
	"\x87\x3dXXXX"           /* xchg   %edi,regs+0x1C */
;

static char *swap_flags =
	"\x50"                   /* push   %eax           */
	"\x9c"                   /* pushf                 */
	"\x58"                   /* pop    %eax           */
	"\x87\x05XXXX"           /* xchg   %eax,regs+0x20 */
	"\x50"                   /* push   %eax           */
	"\x9d"                   /* popf                  */
	"\x58"                   /* pop    %eax           */
;

static char *add_swap_flags(char *code, long *regs)
{
	long *addr = &regs[8];
	memcpy(code, swap_flags, 12);
	memcpy(&code[5], &addr, 4);
	return code+12;
}

static char *add_swap_regs(char *code, long *regs)
{
	int i;
	long *addr;
	memcpy(code, swap_regs, 8*6);
	for (i=0; i<8; i++)
	{
		addr = &regs[i];
		memcpy(&code[2+6*i], &addr, 4);
	}
	return code+8*6;
}

#endif

char *get_rwmem(size_t sz)
{
	return mmap(NULL, sz, PROT_READ|PROT_WRITE,
	                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

void codeexec(char *input, int input_sz, long *regs)
{
	int max_len = strlen(swap_regs)*2 + strlen(swap_flags)*2 + input_sz;
	int sz = ( (max_len-1)|0xfff ) + 1;
	int i;
	char *code = get_rwmem(sz), *p=code;
	func_t func; long intermediate = (long)code;
    func = (func_t)intermediate;


	p = add_swap_flags(p, regs);
	p = add_swap_regs(p, regs);
	memcpy(p, input, input_sz);
	p += input_sz;
	p = add_swap_regs(p, regs);
	p = add_swap_flags(p, regs);

	for (i=14; i; i--)
		*p++ = '\x90'; /* nop */

	*p++ = '\xc3';     /* ret */

	memset(p, 0, sz-(p-code));

	mprotect(code, sz, PROT_READ|PROT_EXEC);
	func();
	munmap(code, sz);
}

void save_fx(char *fx)
{
	asm("fxsave (%0)\n" : : "r"(fx));
}

void load_fx(char *fx)
{
	asm("fxrstor (%0)\n" : : "r"(fx));
}

