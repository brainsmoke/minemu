
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

#include <sys/mman.h>

#define PG_SIZE (0x1000UL)
#define PG_MASK (PG_SIZE-1)
#define PG_SHIFT (12)

#define PAGE_BASE(a) ((long)(a)&~PG_MASK)
#define PAGE_NEXT(a) (PAGE_BASE((a)-1UL)+PG_SIZE)

/*
 * Memory layout:
 *                                                         3G              4G
 * [      USER      ] [ JIT ] [    TAINTING    ] < MINEMU > <    KERNEL   >
 * o_______displacement_______o
 *                    o_______displacement_______x
 *                                             o_______displacement_______x
 * ______o                                            x_______displacement_
 *
 *                                               [  read/write protected  ]
 *
 * USER:
 *
 * [ USER MEMORY:  0x00000000 - 0x49555000 ] \ memory available to user
 * [ USER STACK:   0x49555000 - 0x51555000 ] /
 *
 * JIT:
 *
 * [ RUNTIME CODE: 0x51555000 - 0x515xxxxx ] = user(r-x) minemu(r-x) 
 * [ SCRATCH MEM:  0x515xxxxx - 0x515xxxxx ] = used to save program state.
 *                                             sigaltstack, always writable,
 *                                             should be zeroed out
 * [ RUNTIME DATA: 0x515xxxxx - 0x........ ] = user(r--) minemu(rw-) 
 * [ JIT CODE:     0x........ - 0x57555000 ] = user(r-x) minemu(rw-)
 *
 * TAINT:
 *
 * [ TAINT MEM:    0x57555000 - 0xA8AAA000 ] = user memory + 0x57555000
 *
 * MINEMU:
 *
 * [ MINEMU DATA:    0xA8888888 - 0xC0000000 ] = user(---) minemu(rw-) 
 *
 * (KERNEL) HIGH MEM:
 * if 64 bit kernel:
 * [ RUNTIME DATA: 0xC0000000 -            ] = user(---) minemu(rw-) 
 */

#define HIGH_PAGE (0x100000UL)
/*
#define HIGH_USER_ADDR (0xc0000UL * PG_SIZE)
*/
/* logical sections */

#define JIT_PAGES (0x8000UL)
#define USER_PAGES ( (HIGH_PAGE - 2*JIT_PAGES) /3 )

#define USER_SIZE (USER_PAGES * PG_SIZE)
#define JIT_SIZE (JIT_PAGES * PG_SIZE)
#define TAINT_SIZE (USER_SIZE)
/*#define MINEMU_SIZE (HIGH_USER_ADDR-TAINT_SIZE-JIT_SIZE-USER_SIZE)*/

#define USER_START (0x0UL)
#define USER_END (USER_START+USER_SIZE)
#define JIT_START (USER_END)
#define JIT_END (JIT_START+JIT_SIZE)
#define TAINT_START (JIT_END)
#define TAINT_END (TAINT_START+TAINT_SIZE)
#define MINEMU_START (TAINT_END)
/*#define MINEMU_END (MINEMU_START+MINEMU_SIZE)*/

/* taint offset */

#define TAINT_OFFSET (TAINT_START-USER_START)

/* subsections */

/* info about section sizes we get from the linker
 * some information is doubly represented so we can use them
 * as constants in our mapping tables.
 */
extern char runtime_code_start[], runtime_code_size[],
            runtime_data_start[], runtime_data_size[],
            jit_code_start[], jit_code_size[],
            jit_data_size[],
            fault_page_0[], fault_page_1[], fault_page_2[], fault_page_3[];

#define SYM_VAR(a) ((unsigned long)a)

#define RUNTIME_CODE_START SYM_VAR(runtime_code_start)
#define RUNTIME_CODE_SIZE SYM_VAR(runtime_code_size)

#define RUNTIME_DATA_START SYM_VAR(runtime_data_start)
#define RUNTIME_DATA_SIZE SYM_VAR(runtime_data_size)

#define FAULT_PAGE_0 (fault_page_0)
#define FAULT_PAGE_1 (fault_page_1)
#define FAULT_PAGE_2 (fault_page_2)
#define FAULT_PAGE_3 (fault_page_3)

#define JIT_CODE_START SYM_VAR(jit_code_start)
#define JIT_CODE_SIZE SYM_VAR(jit_code_size)

#define JIT_DATA_SIZE SYM_VAR(jit_data_size)

/* misc */

#define USER_STACK_PAGES (0x8000UL)
#define USER_STACK_SIZE (USER_STACK_PAGES * PG_SIZE)
#define VDSO_SIZE  (PG_SIZE)
#define MINEMU_STACK_BOTTOM (0xBFF70000UL)

void init_minemu_mem(char **envp);

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

void shield(void);
void unshield(void);

void minimal_shield(void);
void minimal_unshield(void);

#endif /* MM_H */
