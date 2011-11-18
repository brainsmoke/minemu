
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
#include <sys/mman.h>
#include <linux/auxvec.h>
#include <errno.h>

#ifndef AT_EXECFN
#define AT_EXECFN 31
#endif

#include "mm.h"
#include "error.h"
#include "lib.h"
#include "syscalls.h"
#include "runtime.h"
#include "jit.h"
#include "codemap.h"
#include "load_elf.h"
#include "kernel_compat.h"

unsigned long vdso, vdso_orig, sysenter_reentry, minemu_stack_bottom;

unsigned long do_mmap2(unsigned long addr, size_t length, int prot,
                       int flags, int fd, off_t pgoffset)
{
	if (length == 0)
		return addr;
	else
		return user_mmap2(addr, length, prot, flags, fd, pgoffset);
}

static unsigned brk_cur = 0, brk_min = 0x10000;

unsigned long set_brk_min(unsigned long new_brk)
{
	if (new_brk > USER_END)
		return -1;

	if (new_brk > brk_min)
		brk_cur = brk_min = new_brk;

	sys_brk(new_brk);

	return brk_cur;
}

unsigned long user_brk(unsigned long new_brk)
{
	if ( (new_brk <= USER_END) && (new_brk >= brk_min) )
	{
		if (new_brk > brk_cur)
			user_mmap2(brk_cur, new_brk-brk_cur,
			           PROT_READ|PROT_WRITE,
			           MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
			           -1, 0);
		else if (new_brk < brk_cur)
			user_munmap(new_brk, brk_cur-new_brk);

		brk_cur = new_brk;
	}

	return brk_cur;
}

unsigned long user_old_mmap(struct kernel_mmap_args *a)
{
	if (a->offset & PG_MASK)
		return -EINVAL;

	return user_mmap2(a->addr, a->len, a->prot, a->flags, a->fd, a->offset >> PG_SHIFT);
}

unsigned long user_mmap2(unsigned long addr, size_t length, int prot,
                         int flags, int fd, off_t pgoffset)
{
	if ( (addr > USER_END) || (addr+length > USER_END) )
		return -EFAULT;

	int new_prot = prot; /* make sure we don't strip implied read permission */
	if (prot & PROT_EXEC)
		new_prot = (prot & ~PROT_EXEC) | PROT_READ;

	unsigned long ret = sys_mmap2(addr, length, new_prot, flags, fd, pgoffset);

	if ( !(ret & PG_MASK) )
	{
		sys_mmap2(ret+TAINT_OFFSET, length, new_prot, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
		if (prot & PROT_EXEC)
		{
			struct kernel_stat64 s;
			if ( (fd < 0) || (sys_fstat64(fd, &s) != 0) )
				memset(&s, 0, sizeof(s));

			add_code_region((char *)ret, PAGE_NEXT(length),
			                s.st_ino, s.st_dev, s.st_mtime, pgoffset);
		}
		else
			del_code_region((char *)ret, PAGE_NEXT(length));
	}

	return ret;
}

unsigned long user_munmap(unsigned long addr, size_t length)
{
	if ( (addr > USER_END) || (addr+length > USER_END) )
		return -EFAULT;

	unsigned long ret = sys_munmap(addr, length);

	if ( !(ret & PG_MASK) )
		del_code_region((char *)addr, PAGE_NEXT(length));

	return ret;
}

unsigned long user_mprotect(unsigned long addr, size_t length, long prot)
{
	if ( (addr > USER_END) || (addr+length > USER_END) )
		return -EFAULT;

	unsigned long ret = sys_mprotect(addr, length, prot&~PROT_EXEC);
	                    sys_mprotect(TAINT_OFFSET+addr, length, prot&~PROT_EXEC);

	if ( !(ret & PG_MASK) )
	{
		if (prot & PROT_EXEC)
			add_code_region((char *)addr, PAGE_NEXT(length), 0, 0, 0, 0);
		else
			del_code_region((char *)addr, PAGE_NEXT(length));
	}

	return ret;
}

void copy_vdso(unsigned long addr, unsigned long orig)
{
	vdso = addr; vdso_orig = orig;

	long ret = user_mmap2(addr, 0x1000, PROT_READ|PROT_WRITE,
	                      MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (ret & PG_MASK)
		die("connot alloc vdso", ret);

	memcpy((char *)vdso, (char *)vdso_orig, 0x1000);

	long off = memscan((char *)vdso, 0x1000, "\x5d\x5a\x59\xc3", 4);

	if (off < 0)
		sysenter_reentry = 0; /* assume int $0x80 syscalls, crash otherwise */
	else
		sysenter_reentry = vdso + off;

	user_mprotect(vdso, 0x1000, PROT_READ|PROT_EXEC);
}

unsigned long stack_top(long *auxv)
{
	return PAGE_NEXT((unsigned long)get_aux(auxv, AT_EXECFN));
}

unsigned long high_user_addr(long *auxv)
{
	return stack_top(auxv) <= 0xC0000000UL ? 0xC0000000UL : 0xFFFFE000UL;
}

static void fill_last_page_hack(void)
{
	char buf[0x2000];
	clear(buf, 0x2000);
}

void init_minemu_mem(long *auxv)
{
	long ret = 0;

	char c[1];

	fill_last_page_hack();

	/* pre-allocate some memory regions, mostly because this way we don't
	 * have to do our own memory-allocation. It /is/ the reason we need
	 * to set vm.overcommit_memory = 1 in sysctl.conf so this might change
	 * in the future (I hope so.)
	 */
	ret |= sys_mmap2(TAINT_START, TAINT_SIZE,
	                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
//	                 PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	ret |= sys_mmap2(JIT_START, JIT_SIZE,
	                 PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	ret |= sys_mmap2(MINEMU_END, PAGE_BASE(c-0x1000)-MINEMU_END,
	                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	ret |= sys_mmap2(MINEMU_END, 0x1000,
	                 PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	fill_last_page_hack();

	if ( high_user_addr(auxv) > stack_top(auxv) )
		ret |= sys_mmap2(stack_top(auxv), high_user_addr(auxv)-stack_top(auxv),
		                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
		                 -1, 0);

	if (ret & PG_MASK)
		die("mem init failed", ret);
}

