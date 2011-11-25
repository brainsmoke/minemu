
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
#include <linux/mman.h>
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
#include "threads.h"

/* switch when shadow shared memory is completely done */
//#define SHADOW_DEFAULT_PROT (PROT_NONE)
#define SHADOW_DEFAULT_PROT (PROT_READ|PROT_WRITE)

unsigned long vdso, vdso_orig, sysenter_reentry, minemu_stack_bottom;

long map_lock;

static int bad_range(unsigned long addr, size_t length)
{
	return (addr > USER_END) || (addr+length > USER_END);
}

static unsigned long min(unsigned long a, unsigned long b) { return a<b ? a:b; }
static unsigned long max(unsigned long a, unsigned long b) { return a>b ? a:b; }

/* make sure we don't strip implied read permission */
static long no_exec(long prot)
{
	int new_prot = prot & ~PROT_EXEC;
	if (prot & PROT_EXEC)
		new_prot |= PROT_READ;
	return new_prot;
}

static void shadow_mmap(unsigned long addr, size_t length, long prot, int fd, off_t pgoffset)
{
	long ret;

	if (length == 0)
		return;

	ret = sys_mmap2(addr+TAINT_OFFSET, length, no_exec(prot),
	                MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (ret & PG_MASK)
		die("shadow_m{,un}map(): %08x\n", ret);

	if (prot & PROT_EXEC)
	{
		struct kernel_stat64 s;
		if ( (fd < 0) || (sys_fstat64(fd, &s) != 0) )
			memset(&s, 0, sizeof(s));

		add_code_region((char *)addr, PAGE_NEXT(length),
		                s.st_ino, s.st_dev, s.st_mtime, pgoffset);
	}
	else
		del_code_region((char *)addr, PAGE_NEXT(length));
}

static void shadow_munmap(unsigned long addr, size_t length)
{
	shadow_mmap(addr, length, SHADOW_DEFAULT_PROT, -1, 0);
}

/* mremap() is less orthogonal than one would expect :-( luckily we only need
 * to handle success.
 */
static void shadow_mremap(unsigned long old_addr, size_t old_size,
                          size_t new_size, long _flags, unsigned long new_addr)
{
	long flags = (old_addr != new_addr) ? MREMAP_MAYMOVE|MREMAP_FIXED : 0;
	int is_code = !!find_code_map((char *)old_addr);

	if (new_addr < old_addr)
		shadow_munmap(new_addr, min(old_addr-new_addr, new_size));

	if (new_addr+new_size > old_addr+old_size)
		if (flags)
			shadow_munmap(max(new_addr, old_addr+old_size),
			              min(new_size, new_addr+new_size-old_addr-old_size));
		else if (new_size != old_size)
		{
			mutex_lock(&map_lock); /* make sure user_mmap2() will not return memory
			                        * from our shadow memory hole.
			                        */
			sys_munmap(old_addr+old_size+TAINT_OFFSET, new_size-old_size);
		}

	if ( (new_addr != old_addr) || (new_size != old_size) )
	{
		long ret = sys_mremap(old_addr+TAINT_OFFSET, old_size, new_size, flags,
		                      new_addr+TAINT_OFFSET);

		if (!flags)
			mutex_unlock(&map_lock); /* release lock before we may do other locking
			                          * to prevent deadlocks.
			                          */

		if (ret & PG_MASK)
			die("shadow_mremap(): %08x\n", ret);

		if (is_code)
			add_code_region((char *)new_addr, PAGE_NEXT(new_size), 0, 0, 0, 0);
		else
			del_code_region((char *)new_addr, PAGE_NEXT(new_size));
	}

	if (old_addr < new_addr)
		shadow_munmap(old_addr, min(new_addr-old_addr, old_size));

	if (old_addr+old_size > new_addr+new_size)
		shadow_munmap(max(old_addr, new_addr+new_size),
		              min(old_size, old_addr+old_size-new_addr-new_size));
}

static void shadow_shmat(unsigned long shmaddr)
{
	/* shit, no size known :-(( parse /proc/self/maps to get the size */
	map_file_t f;
	map_entry_t e;
	open_maps(&f);
	while (read_map(&f, &e))
		if (e.addr == shmaddr)
		{
			shadow_mmap(e.addr, e.len, PROT_READ|PROT_WRITE, 0, 0);
			break;
		}
	close_maps(&f);
}

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
	if ( bad_range(addr, length) )
		return -EFAULT;

	/* shadow_mremap() might temporarily leave holes in shadow memory
	 * make sure we won't get this memory.
	 */
	mutex_lock(&map_lock);
	unsigned long ret = sys_mmap2(addr, length, no_exec(prot),
	                              flags, fd, pgoffset);
	mutex_unlock(&map_lock);

	if ( !(ret & PG_MASK) )
		shadow_mmap(ret, length, prot, fd, pgoffset);

	return ret;
}

unsigned long user_munmap(unsigned long addr, size_t length)
{
	if ( bad_range(addr, length) )
		return -EFAULT;

	unsigned long ret = sys_munmap(addr, length);

	if ( !(ret & PG_MASK) )
		shadow_munmap(addr, PAGE_NEXT(length));

	return ret;
}

unsigned long user_mprotect(unsigned long addr, size_t length, long prot)
{
	if ( bad_range(addr, length) )
		return -EFAULT;

	unsigned long ret = sys_mprotect(addr, length, no_exec(prot));
	                    sys_mprotect(TAINT_OFFSET+addr, length, no_exec(prot));

	if ( !(ret & PG_MASK) )
	{
		if (prot & PROT_EXEC)
			add_code_region((char *)addr, PAGE_NEXT(length), 0, 0, 0, 0);
		else
			del_code_region((char *)addr, PAGE_NEXT(length));
	}

	return ret;
}

unsigned long user_madvise(unsigned long addr, size_t length, long advise)
{
	if ( bad_range(addr, length) )
		return -ENOMEM;
	
	unsigned long ret = sys_madvise(addr, length, advise);

	if (!ret && advise == MADV_DONTNEED)
		sys_madvise(TAINT_OFFSET+addr, length, advise);

	return ret;
}

unsigned long user_shmat(int shmid, char *shmaddr, int shmflg, unsigned long *raddr)
{
	unsigned long ret = sys_shmat(shmid, shmaddr, shmflg, raddr);

	if ( !ret )
		shadow_shmat(*raddr);

	return ret;
}

unsigned long user_mremap(unsigned long old_addr, size_t old_size,
                          size_t new_size, long flags, unsigned long new_addr)
{
	if ( bad_range(old_addr, old_size) )
		return -ENOMEM;

	if ( (flags & MREMAP_FIXED) && bad_range(new_addr, new_size) )
		return -ENOMEM;
	
	unsigned long ret = sys_mremap(old_addr, old_size, new_size, flags, new_addr);

	if (! (ret & PG_MASK) )
		shadow_mremap(old_addr, old_size, new_size, flags, ret);

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
	mutex_init(&map_lock);

	/* pre-allocate some memory regions, mostly because this way we don't
	 * have to do our own memory-allocation. It /is/ the reason we need
	 * to set vm.overcommit_memory = 1 in sysctl.conf so this might change
	 * in the future (I hope so.)
	 */
	ret |= sys_mmap2(TAINT_START, PG_SIZE,
	                 PROT_NONE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	ret |= sys_mmap2(TAINT_START+PG_SIZE, TAINT_SIZE-PG_SIZE,
	                 SHADOW_DEFAULT_PROT, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
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

