
#include <linux/unistd.h>
#include <string.h>

#include "scratch.h"
#include "runtime.h"
#include "jit.h"
#include "mm.h"
#include "error.h"
#include "syscalls.h"
#include "sigwrap.h"
#include "exec.h"
#include "taint.h"

long syscall_emu(long call, long arg1, long arg2, long arg3,
                            long arg4, long arg5, long arg6)
{
/*
	debug("eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x "
          "ebp %08x esp %08x eip %08x",
	      call, arg1, arg2, arg3, arg4, arg5, arg6);
*/

	long ret;
	switch (call)
	{
 		case __NR_brk:
 		case __NR_mmap2:
 		case __NR_mprotect:

 		case __NR_sigaltstack:
 		case __NR_signal:
 		case __NR_sigaction:
		case __NR_sigreturn:
 		case __NR_rt_sigaction:
		case __NR_rt_sigreturn:
		case __NR_execve:
			break;

		case __NR_read:
		case __NR_readv:
		case __NR_open:
		case __NR_creat:
		case __NR_dup:
		case __NR_dup2:
		case __NR_openat:
		case __NR_pipe:
		case __NR_socketcall:
			ret = syscall_intr(call,arg1,arg2,arg3,arg4,arg5,arg6);
			do_taint(ret,call,arg1,arg2,arg3,arg4,arg5,arg6);
			return ret;
		case __NR_vfork:
			call = __NR_fork;
		default:
			return syscall_intr(call,arg1,arg2,arg3,arg4,arg5,arg6);
	}

	ret = call;
	if (!try_block_signals())
		return ret; /* we have a signal in progress, revert to pre-syscall state */

	minimal_unshield();

	switch (call)
	{
		/* these calls are all non-blocking right?
		 * blocked signals during blocking calls is a bad thing
		 */
 		case __NR_brk:
			ret = user_brk(arg1);
			break;
 		case __NR_mmap2:
			ret = user_mmap2(arg1,arg2,arg3,arg4,arg5,arg6);
			break;
 		case __NR_mprotect:
			ret = user_mprotect(arg1,arg2,arg3);
			break;

 		case __NR_sigaltstack:
			ret = user_sigaltstack((stack_t *)arg1, (stack_t *)arg2);
			break;
 		case __NR_signal:
		{
			ret = (long)user_signal(arg1, (kernel_sighandler_t)arg2);
			break;
		}
 		case __NR_sigaction:
			ret = user_sigaction(arg1, (struct kernel_old_sigaction *)arg2,
			                           (struct kernel_old_sigaction *)arg3);
			break;
 		case __NR_rt_sigaction:
			ret = user_rt_sigaction(arg1, (struct kernel_sigaction *)arg2,
			                              (struct kernel_sigaction *)arg3, arg4);
			break;
 		case __NR_sigreturn:
			shield();
			user_sigreturn();
			break;
 		case __NR_rt_sigreturn:
			shield();
			user_rt_sigreturn();
			break;
		case __NR_execve:
			unshield();
			ret = user_execve((char *)arg1, (char **)arg2, (char **)arg3);
			shield();
			break;
		default:
			die("unimplemented syscall");
			break;
	}

	minimal_shield();
	unblock_signals();
	return ret;
}

