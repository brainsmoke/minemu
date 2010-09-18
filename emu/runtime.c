#include <sys/mman.h>

#include "syscalls.h"
#include "runtime.h"
#include "mm.h"
#include "jmpcache.h"

long (*runtime_ijmp_addr)(void) = runtime_ijmp;
long (*int80_emu_addr)(void) = int80_emu;
long (*linux_sysenter_emu_addr)(void) = linux_sysenter_emu;

char syscall_hooks[N_SYSCALL_HOOKS] =
{
	[__NR_brk      ] = EMU_BIT,
	[__NR_mmap     ] = EMU_BIT,
	[__NR_mmap2    ] = EMU_BIT,
	[__NR_mprotect ] = EMU_BIT,
/*
	[__NR_signal       ] = EMU_BIT,
	[__NR_sigaction    ] = EMU_BIT,
	[__NR_sigreturn    ] = EMU_BIT,
	[__NR_rt_sigaction ] = EMU_BIT,
	[__NR_rt_sigreturn ] = EMU_BIT,
*/};

