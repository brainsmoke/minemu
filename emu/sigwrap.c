/*
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <stdlib.h>
#include <stddef.h>

#include "syscalls.h"
#include "error.h"
#include "sigwrap.h"

static void *pre_signal_stack, *post_signal_stack;

static void print_stackframe(void)
{
	sys_write(1, post_signal_stack, (long)pre_signal_stack - (long)post_signal_stack);
}

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

void get_registers(pid_t pid, struct user_regs_struct *regs)
{
    if ( sys_ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0 )
        die("error: getting registers failed for process %d", pid);
}

void set_registers(pid_t pid, struct user_regs_struct *regs)
{
    if ( sys_ptrace(PTRACE_SETREGS, pid, 0, &regs) < 0 )
        die("error: setting registers failed for process %d", pid);
}

void get_siginfo(pid_t pid, siginfo_t *info)
{
	if ( sys_ptrace(PTRACE_GETSIGINFO, pid, 0, info) < 0 )
		die("error: getting signal info failed for process %d", pid);
}

void set_siginfo(pid_t pid, siginfo_t *info)
{
	if ( sys_ptrace(PTRACE_SETSIGINFO, pid, 0, info) < 0 )
		die("error: setting signal info failed for process %d", pid);
}

static void print_debug_regs(pid_t pid)
{
	int i;
	long dr=9;

	for (i=0; i<8; i++)
	{
		sys_ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[i]), &dr);
		debug("DR%d %d", i, dr);
	}
}

static void signal_translate_pc(pid_t pid, int signum)
{
	siginfo_t info;
	struct user_regs_struct regs;

print_debug_regs(pid);
debug("signal: pid=%d", pid, regs.eip, regs.esp);
	get_siginfo(pid, &info);
	get_registers(pid, &regs);
debug("signal: pid=%d eip=%d esp=%d", pid, regs.eip, regs.esp);
	set_registers(pid, &regs);
	set_siginfo(pid, &info);
	sys_ptrace(PTRACE_SINGLESTEP, pid, 0, signum);
	sys_ptrace(PTRACE_CONT, pid, 0, 0);
}

static void trace(pid_t pid)
{
	int status, signum;

	while (sys_ptrace(PTRACE_SETOPTIONS, pid, -1,
	                  PTRACE_O_TRACEFORK |
	                  PTRACE_O_TRACEVFORK |
	                  PTRACE_O_TRACECLONE |
	                  PTRACE_O_TRACESYSGOOD) != 0);

	pid = sys_waitpid(-1, &status, __WALL);

	status|=0x80<<8;
	while (pid > 0)
	{
		signum = (status>>8) & 0xff;
		if (signum & 0x80)
			signum = 0;

print_debug_regs(pid);
//		if (signum)
//			signal_translate_pc(pid, signum);

		sys_ptrace(PTRACE_CONT, pid, 0, signum);
//		sys_ptrace(PTRACE_CONT, pid, 0, 0);

		pid = sys_waitpid(-1, &status, __WALL);
	}
}

void temu_signal_wrapper(void)
{
	pid_t pid = sys_fork();

	if (pid > 0) /* parent process *
	{
		trace(pid);
		sys_exit(EXIT_SUCCESS);
	}

	if (pid < 0)
		die("fork failed");
	else if ( sys_ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1 )
		die("trace failed");
}
*/
