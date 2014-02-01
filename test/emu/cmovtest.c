
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
#include <signal.h>

#include "debug.h"
#include "error.h"
#include "taint_code.h"
#include "taint.h"
#include "opcodes.h"
#include "jit_code.h"
#include "threads.h"

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

/* make symbols resolve */
void runtime_ijmp(void) { die("calling placeholder"); }
void runtime_ret(void) { die("calling placeholder"); }
void runtime_ret_cleanup(void) { die("calling placeholder"); }
void int80_emu(void) { die("calling placeholder"); }
void linux_sysenter_emu(void) { die("calling placeholder"); }
void jit_rev_lookup_addr(void) { die("calling placeholder"); }
void unshield(void) { die("calling placeholder"); }
char *hexcat(char *dest, unsigned long ul) { die("calling placeholder"); return NULL; }
void jit_return(void) { die("calling placeholder"); }
void jit_fragment_exit(void) { die("calling placeholder"); }
void cpuid_emu(void) { die("calling placeholder"); }
void clear_jmp_cache(void) { die("calling placeholder"); }
void hook_stub(void) { die("calling placeholder"); }

void altstack_setup(void){}

char user_sigaction_list[1];
int taint_flag = TAINT_ON;

typedef long (*func_t)(long flags, long orig, long condval);

long test_cmovcc(int cond, long flags, long orig, long condval)
{
	char *code = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	long intermediate = (long)code;
	func_t func = (func_t) intermediate;
	gen_code(
		code,

		"8B 44 24 04"     /* mov     4(%esp), %eax  */
		"50"              /* push    %eax           */
		"9D"              /* popf                   */
		"8B 44 24 08"     /* mov     8(%esp), %eax  */
		"0F . 44 24 0C"   /* cmovx  12(%esp), %eax  */
		"C3",             /* ret                    */

		0x40 | (cond & 0xf)
	);

    mprotect(code, 4096, PROT_READ|PROT_EXEC);
	long ret = func(flags, orig, condval);

    munmap(code, 4096);
	return ret;
}

#define SET(f) ( (flags & (f)) ? 1 : 0 )
#define EQUAL(f_a, f_b) ( SET(f_a) == SET(f_b) ? 1 : 0 )
#define DIFFER(f_a, f_b) ( SET(f_a) != SET(f_b) ? 1 : 0 )

int main(int argc, char **argv)
{
	long flags;
	enum { CF=1, PF=4, AF=16, ZF=64, SF=128, OF=2048 };
	init_threads();

	for (flags=1; flags<0x1000; flags++) if ( (flags & 0x72A) == 0x02 )
	{
		if ( SET(OF) != test_cmovcc(0, flags, 0, 1) )
			printf("%d %ld\n", 0, flags);

		if ( SET(OF) == test_cmovcc(1, flags, 0, 1) )
			printf("%d %ld\n", 1, flags);

		if ( SET(CF) != test_cmovcc(2, flags, 0, 1) )
			printf("%d %ld\n", 2, flags);

		if ( SET(CF) == test_cmovcc(3, flags, 0, 1) )
			printf("%d %ld\n", 3, flags);

		if ( SET(ZF) != test_cmovcc(4, flags, 0, 1) )
			printf("%d %ld\n", 4, flags);

		if ( SET(ZF) == test_cmovcc(5, flags, 0, 1) )
			printf("%d %ld\n", 5, flags);

		if ( SET(CF|ZF) != test_cmovcc(6, flags, 0, 1) )
			printf("%d %ld\n", 6, flags);

		if ( SET(CF|ZF) == test_cmovcc(7, flags, 0, 1) )
			printf("%d %ld\n", 7, flags);

		if ( SET(SF) != test_cmovcc(8, flags, 0, 1) )
			printf("%d %ld\n", 8, flags);

		if ( SET(SF) == test_cmovcc(9, flags, 0, 1) )
			printf("%d %ld\n", 9, flags);

		if ( SET(PF) != test_cmovcc(10, flags, 0, 1) )
			printf("%d %ld\n", 10, flags);

		if ( SET(PF) == test_cmovcc(11, flags, 0, 1) )
			printf("%d %ld\n", 11, flags);

		if ( DIFFER(SF,OF) != test_cmovcc(12, flags, 0, 1) )
			printf("%d %ld\n", 12, flags);

		if ( DIFFER(SF,OF) == test_cmovcc(13, flags, 0, 1) )
			printf("%d %ld\n", 13, flags);

		if ( (SET(ZF) | DIFFER(SF,OF)) != test_cmovcc(14, flags, 0, 1) )
			printf("%d %ld\n", 14, flags);

		if ( (SET(ZF) | DIFFER(SF,OF)) == test_cmovcc(15, flags, 0, 1) )
			printf("%d %ld\n", 15, flags);
	}
}
