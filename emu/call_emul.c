
#include <linux/unistd.h>

#include "scratch.h"
#include "runtime.h"
#include "jit.h"
#include "mm.h"
#include "error.h"
#include "syscalls.h"

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
			unshield();
			ret = user_brk(arg1);
			shield();
			jit_eip = 0;
			break;
 		case __NR_mmap2:
			unshield();
			ret = user_mmap2(arg1,arg2,arg3,arg4,arg5,arg6);
			shield();
			jit_eip = 0;
			break;
 		case __NR_mprotect:
			unshield();
			ret = user_mprotect(arg1,arg2,arg3);
			shield();
			jit_eip = 0;
			break;
		default:
			ret = syscall6(call,arg1,arg2,arg3,arg4,arg5,arg6);
	}

	return ret;
}

