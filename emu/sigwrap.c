
#include <stdlib.h>
#include <string.h>

#include "syscalls.h"
#include "sigwrap.h"

int is_synchronous(siginfo_t *info)
{
    switch (info->si_signo)
    {
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGSEGV:
        case SIGSTKFLT:
            if ( info->si_code != SI_KERNEL && info->si_code > 0 )
                return 1;
        default:
            return 0;
    }
}

extern sigset_t old_sigset;

extern char syscall_intr_critical_start[], syscall_intr_critical_end[],
            syscall_intr_call_return[],
            runtime_ijmp_critical_start[], runtime_ijmp_critical_end[];

int try_block_signals(void)
{
	sigset_t blockall;
	//sigemptyset(&blockall);
	memset(&blockall, 0, sizeof(sigset_t));
	return syscall_intr(__NR_sigprocmask, (long)&blockall, (long)&old_sigset, 0,0,0,0);
}

void unblock_signals(void)
{
	syscall_intr(__NR_sigprocmask, (long)&old_sigset, (long)NULL, 0,0,0,0);
}

