
#include <linux/unistd.h>

#include "scratch.h"
#include "runtime.h"
#include "jit.h"
#include "mm.h"
#include "error.h"
#include "syscalls.h"
#include "sigwrap.h"

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
			break;
		default:
			return syscall_intr(call,arg1,arg2,arg3,arg4,arg5,arg6);
	}

	long ret = -1;

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
		default:
			break;
	}

	shield();
	unblock_signals();
	jit_eip = 0;

	return ret;
}

