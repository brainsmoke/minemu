
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

#ifndef MM_H
#define MM_H

#define PG_SIZE (0x1000UL)
#define PG_MASK (PG_SIZE-1)
#define PG_SHIFT (12)

#define PAGE_BASE(a) ((long)(a)&~PG_MASK)
#define PAGE_NEXT(a) (PAGE_BASE((a)-1UL)+PG_SIZE)

#define HIGH_PAGE (0x100000UL)
//#define USER_PAGES ( (HIGH_PAGE) /3 )
#define USER_PAGES ( 0x50000 )
#define JIT_PAGES (0x14000UL)

#define USER_SIZE (USER_PAGES * PG_SIZE)
#define TAINT_SIZE (USER_SIZE)
#define JIT_SIZE (JIT_PAGES * PG_SIZE)

#define USER_START (0x0UL)
#define USER_END (USER_START+USER_SIZE)
#define TAINT_START (USER_END)
#define TAINT_END (TAINT_START+TAINT_SIZE)
#define JIT_START (TAINT_END)
#define JIT_END (JIT_START+JIT_SIZE)
#define MINEMU_START (JIT_END)

/* the linker will tell us where minemu's allocated memory ends */
extern char minemu_end[], minemu_code_start[], minemu_code_end[];
#define MINEMU_END ((unsigned long)minemu_end)

/* taint offset */

#define TAINT_OFFSET (TAINT_START-USER_START)

/* misc */

#define USER_STACK_PAGES (0x6000UL)
#define USER_STACK_SIZE (USER_STACK_PAGES * PG_SIZE)
#define VDSO_SIZE  (PG_SIZE)

#include <sys/mman.h>

extern unsigned long vdso, vdso_orig, sysenter_reentry, minemu_stack_bottom;

void init_minemu_mem(long *auxv);

unsigned long set_brk_min(unsigned long brk);

unsigned long do_mmap2(unsigned long addr, size_t length, int prot,
                       int flags, int fd, off_t pgoffset);

unsigned long user_brk(unsigned long brk);

struct kernel_mmap_args
{
    unsigned long addr;
    unsigned long len;
    unsigned long prot;
    unsigned long flags;
    unsigned long fd;
    unsigned long offset;
};

unsigned long user_old_mmap(struct kernel_mmap_args *a);

unsigned long user_mmap2(unsigned long addr, size_t length, int prot,
                         int flags, int fd, off_t pgoffset);

unsigned long user_munmap(unsigned long addr, size_t length);

unsigned long user_mprotect(unsigned long addr, size_t length, long prot);
unsigned long user_madvise(unsigned long addr, size_t length, long advise);
unsigned long user_mremap(unsigned long old_addr, size_t old_size,
                          size_t new_size, long flags, unsigned long new_addr);

void copy_vdso(unsigned long addr, unsigned long orig);

void shield(void);
void unshield(void);

#endif /* MM_H */
