#include <sys/mman.h>
#include <linux/personality.h>

#include "syscalls.h"
#include "error.h"
#include "load_elf.h"
#include "lib.h"
#include "error.h"
#include "mm.h"
#include "runtime.h"
#include "scratch.h"
#include "jit.h"
#include "codemap.h"
//#include "opcodes.h"
#include "sigwrap.h"

int temu_main(int argc, char **argv, char **envp, long *auxv)
{
	unsigned long pers = sys_personality(0xffffffff);

	if (ADDR_COMPAT_LAYOUT & ~pers)
	{
		sys_personality(ADDR_COMPAT_LAYOUT | pers);
//		temu_signal_wrapper();
		sys_execve("/proc/self/exe", argv, envp);
	}

	init_temu_mem(envp);

	elf_prog_t prog =
	{
		.filename = argv[1],
		.argv = &argv[1],
		.envp = envp,
		.auxv = auxv,
		.task_size = USER_END,
		.stack_size = USER_STACK_SIZE,
	};

	int ret = load_elf(&prog);
	if (ret < 0)
		die("load_elf", ret);

	jit_init();

	char *vdso = (char *)get_aux(prog.auxv, AT_SYSINFO_EHDR);
	long off = memscan(vdso, 0x1000, "\x5d\x5a\x59\xc3", 4);

	if (off < 0)
		die("unable to guess sysenter re-entry");

	sysenter_reentry = (long)&vdso[off];

	add_code_region(vdso, 0x1000); /* vdso */

	long eip = (long)jit(prog.entry);
	long esp = (long)prog.sp;

	enter(eip, esp);

	sys_exit(1);
	return 1;
}
