#include <string.h>
#include <sys/mman.h>
#include <linux/auxvec.h>
#include <errno.h>

#ifndef AT_EXECFN
#define AT_EXECFN 31
#endif
#ifndef AT_BASE_PLATFORM
#define AT_BASE_PLATFORM 24
#endif

#include "mm.h"
#include "error.h"
#include "lib.h"
#include "syscalls.h"
#include "runtime.h"
#include "jit.h"
#include "codemap.h"

typedef struct
{
	unsigned long start;
	unsigned long length;
	long prot;

} mem_map_t;

static mem_map_t shield_maps[] =
{
	{ .start = RUNTIME_DATA_START, .length = RUNTIME_DATA_SIZE, .prot = PROT_READ            },
	{ .start = JIT_CODE_START,     .length = JIT_CODE_SIZE,     .prot = PROT_READ|PROT_EXEC  },
	/* modified by init_temu: */
	{ .start = TAINT_END,          .length = 0,                 .prot = PROT_NONE            },
	{ .start = 0 },
};

static mem_map_t unshield_maps[] =
{
/*	{ .start = RUNTIME_DATA_START, .length = RUNTIME_DATA_SIZE, .prot = PROT_READ|PROT_WRITE },
	{ .start = JIT_CODE_START,     .length = JIT_CODE_SIZE,     .prot = PROT_READ|PROT_WRITE },*/
	{ .start = RUNTIME_DATA_START, .length = JIT_DATA_SIZE,     .prot = PROT_READ|PROT_WRITE },
	/* modified by init_temu: */
	{ .start = TAINT_END,          .length = 0,                 .prot = PROT_READ|PROT_WRITE },
	{ .start = 0 },
};


unsigned long do_mmap2(unsigned long addr, size_t length, int prot,
                       int flags, int fd, off_t pgoffset)
{
	if (length == 0)
		return addr;
	else
		return user_mmap2(addr, length, prot, flags, fd, pgoffset);
}

static unsigned brk_cur = 0, brk_min = 0;

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

unsigned long user_mmap2(unsigned long addr, size_t length, int prot,
                         int flags, int fd, off_t pgoffset)
{
	if ( (addr > USER_END) || (addr+length > USER_END) )
		return -EFAULT;

	unsigned long ret = sys_mmap2(addr, length, prot&~PROT_EXEC, flags, fd, pgoffset);

	if ( !(ret & PG_MASK) )
	{
		if (prot & PROT_EXEC)
			add_code_region((char *)ret, PAGE_NEXT(length));
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

	unsigned long ret = sys_mprotect(addr, length, prot);

	if ( !(ret & PG_MASK) )
		del_code_region((char *)addr, PAGE_NEXT(length));

	return ret;
}

unsigned long stack_top(char **envp)
{
	unsigned long max = 0;
	for ( ; *envp ; envp++ )
		if ( (unsigned long)*envp > max )
			max = (unsigned long)*envp;

	return PAGE_NEXT((unsigned long)max);
}

unsigned long high_user_addr(char **envp)
{
	return stack_top(envp) <= 0xC0000000 ? 0xC0000000 : 0xFFFFE000;
}

static void fill_last_page_hack(void)
{
	char buf[0x2000];
	memset(buf, 0, 0x2000);
}

void set_protection(mem_map_t *maps)
{
	mem_map_t *i;

	for (i=maps; i->start; i++)
		if (sys_mprotect(i->start, i->length, i->prot))
			die("failed to give memory region %x with size %x protection %x",
			    i->start, i->length, i->prot);
}

void shield(void)
{
	set_protection(shield_maps);
}

void unshield(void)
{
	set_protection(unshield_maps);
}

void init_temu_mem(char **envp)
{
	long ret = 0;

	char c[1];

	fill_last_page_hack();

	ret |= sys_mmap2(TAINT_END, PAGE_BASE(c-0x1000)-TAINT_END,
	                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	fill_last_page_hack();

	ret |= sys_mmap2(JIT_CODE_START, JIT_CODE_SIZE,
	                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	ret |= sys_mmap2(TAINT_START, TAINT_SIZE,
	                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
	                 -1, 0);

	if ( high_user_addr(envp) > stack_top(envp) )
		ret |= sys_mmap2(stack_top(envp), high_user_addr(envp)-stack_top(envp),
		                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS,
		                 -1, 0);

	ret |= sys_munmap(FAULT_PAGE_0, PG_SIZE);
	ret |= sys_munmap(FAULT_PAGE_1, PG_SIZE);

	mem_map_t *i;

	for (i=shield_maps; i->start; i++)
		if (i->start == TAINT_END)
			i->length = high_user_addr(envp)-TAINT_END;

	for (i=unshield_maps; i->start; i++)
		if (i->start == TAINT_END)
			i->length = high_user_addr(envp)-TAINT_END;

	if (ret & PG_MASK)
		die("mem init failed", ret);
}

