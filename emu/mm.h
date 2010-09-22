#ifndef MM_H
#define MM_H

#include <sys/mman.h>

#define PG_SIZE (0x1000UL)
#define PG_MASK (PG_SIZE-1)

#define PAGE_BASE(a) ((long)(a)&~PG_MASK)
#define PAGE_NEXT(a) (PAGE_BASE((a)-1UL)+PG_SIZE)

/*
 * Memory layout:
 *                                                         3G              4G
 * [      USER      ] [ JIT ] [    TAINTING    ] <  TEMU  > <    KERNEL   >
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
 * [ RUNTIME CODE: 0x51555000 - 0x515xxxxx ] = user(r-x) temu(r-x) 
 * [ SCRATCH MEM:  0x515xxxxx - 0x515xxxxx ] = used to save program state.
 *                                             sigaltstack, always writable,
 *                                             should be zeroed out
 * [ RUNTIME DATA: 0x515xxxxx - 0x........ ] = user(r--) temu(rw-) 
 * [ JIT CODE:     0x........ - 0x57555000 ] = user(r-x) temu(rw-)
 *
 * TAINT:
 *
 * [ TAINT MEM:    0x57555000 - 0xA8AAA000 ] = user memory + 0x57555000
 *
 * TEMU:
 *
 * [ TEMU CODE:    0xA8AAA000 - 0x........ ] = user(---) temu(r-x) 
 * [ TEMU DATA:    0x........ - 0xC0000000 ] = user(---) temu(rw-) 
 *
 * (KERNEL) HIGH MEM:
 * if 64 bit kernel:
 * [ RUNTIME DATA: 0xC0000000 -            ] = user(---) temu(rw-) 
 */

#define HIGH_PAGE (0x100000UL)
/*
#define HIGH_USER_ADDR (0xc0000UL * PG_SIZE)
*/
/* logical sections */

#define JIT_PAGES (0x6000UL)
#define USER_PAGES ( (HIGH_PAGE - 2*JIT_PAGES) /3 )

#define USER_SIZE (USER_PAGES * PG_SIZE)
#define JIT_SIZE (JIT_PAGES * PG_SIZE)
#define TAINT_SIZE (USER_SIZE)
/*#define TEMU_SIZE (HIGH_USER_ADDR-TAINT_SIZE-JIT_SIZE-USER_SIZE)*/

#define USER_START (0x0UL)
#define USER_END (USER_START+USER_SIZE)
#define JIT_START (USER_END)
#define JIT_END (JIT_START+JIT_SIZE)
#define TAINT_START (JIT_END)
#define TAINT_END (TAINT_START+TAINT_SIZE)
#define TEMU_START (TAINT_END)
/*#define TEMU_END (TEMU_START+TEMU_SIZE)*/

/* taint offset */

#define TAINT_OFFSET (TAINT_START-USER_START)

/* subsections */

/* info about section sizes we get from the linker
 * some information is doubly represented so we can use them
 * as constants in our mapping tables.
 */
/*
extern char runtime_text_end, runtime_text_size, temu_text_end, temu_text_size,
            runtime_data_end, runtime_data_size, temu_data_end, temu_data_size,
            jit_code_size;
*/
extern char runtime_data_start[], runtime_data_size[],
            jit_code_start[], jit_code_size[],
            jit_data_size[],
            fault_page_0[], fault_page_1[], fault_page_2[];

#define SYM_VAR(a) ((unsigned long)a)

#define RUNTIME_DATA_START SYM_VAR(runtime_data_start)
#define RUNTIME_DATA_SIZE SYM_VAR(runtime_data_size)

#define FAULT_PAGE_0 (fault_page_0)
#define FAULT_PAGE_1 (fault_page_1)
#define FAULT_PAGE_2 (fault_page_2)

#define JIT_CODE_START SYM_VAR(jit_code_start)
#define JIT_CODE_SIZE SYM_VAR(jit_code_size)

#define JIT_DATA_SIZE SYM_VAR(jit_data_size)

/* misc */

#define USER_STACK_PAGES (0x8000UL)
#define USER_STACK_SIZE (USER_STACK_PAGES * PG_SIZE)
#define VDSO_SIZE  (PG_SIZE)
#define TEMU_STACK_BOTTOM (0xBFFF7000UL)

void init_temu_mem(char **envp);

unsigned long set_brk_min(unsigned long brk);

unsigned long do_mmap2(unsigned long addr, size_t length, int prot,
                       int flags, int fd, off_t pgoffset);

unsigned long user_brk(unsigned long brk);

unsigned long user_mmap2(unsigned long addr, size_t length, int prot,
                         int flags, int fd, off_t pgoffset);

unsigned long user_munmap(unsigned long addr, size_t length);

unsigned long user_mprotect(unsigned long addr, size_t length, long prot);

void shield(void);
void unshield(void);

#endif /* MM_H */
