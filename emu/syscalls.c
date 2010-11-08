
#include <linux/unistd.h>

#include "scratch.h"
#include "runtime.h"
#include "jit.h"
#include "mm.h"
#include "error.h"
#include "syscalls.h"
#include "sigwrap.h"
#include "exec.h"

long syscall_emu(long call, long arg1, long arg2, long arg3,
                            long arg4, long arg5, long arg6)
{
/*
	debug("eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x "
          "ebp %08x esp %08x eip %08x",
	      call, arg1, arg2, arg3, arg4, arg5, arg6);
*/

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
		case __NR_vfork:
			call = __NR_fork;
		default:
			return syscall_intr(call,arg1,arg2,arg3,arg4,arg5,arg6);
	}

	long ret = call;

	if (!try_block_signals())
		return ret; /* we have a signal in progress, revert to pre-syscall state */

	unshield();

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
			kernel_sighandler_t handler;
			*(long*)&handler = arg2;
			ret = (long)user_signal(arg1, handler);
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
			ret = user_execve((char *)arg1, (char **)arg2, (char **)arg3);
			break;
		default:
			die("unimplemented syscall");
			break;
	}

	shield();
	unblock_signals();
	return ret;
}

