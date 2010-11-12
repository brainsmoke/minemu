#include <sys/mman.h>
#include <linux/personality.h>
#include <string.h>

#include "syscalls.h"
#include "error.h"
#include "load_elf.h"
#include "lib.h"
#include "error.h"
#include "mm.h"
#include "runtime.h"
#include "scratch.h"
#include "jit.h"
#include "jit_cache.h"
#include "codemap.h"
#include "sigwrap.h"

char **parse_options(char **argv)
{
	for (;;)
	{
		if ( (*argv == NULL) || (**argv != '-') )
			return argv;

		if ( strcmp(*argv, "--") == 0 )
			return argv+1;

		if ( strcmp(*argv, "-cache") == 0 )
			set_jit_cache_dir(*++argv);
		else
			die("unknown option: %s", *argv);

		argv++;
	}
}

/* not called main() to avoid warnings about extra parameters :-(  */
int temu_main(int argc, char *argv[], char **envp, long *auxv)
{
	unsigned long pers = sys_personality(0xffffffff);

	if (ADDR_COMPAT_LAYOUT & ~pers)
	{
		sys_personality(ADDR_COMPAT_LAYOUT | pers);
		sys_execve("/proc/self/exe", argv, envp);
	}

	argv = parse_options(&argv[1]);

	init_temu_mem(envp);
	sigwrap_init();
	jit_init();

	elf_prog_t prog =
	{
		.filename = argv[0],
		.argv = &argv[1],
		.envp = envp,
		.auxv = auxv,
		.task_size = USER_END,
		.stack_size = USER_STACK_SIZE,
	};

	int ret = load_elf(&prog);
	if (ret < 0)
		die("load_elf: %d", ret);

	char *vdso = (char *)get_aux(prog.auxv, AT_SYSINFO_EHDR);
	long off = memscan(vdso, 0x1000, "\x5d\x5a\x59\xc3", 4);

	if (off < 0)
		sysenter_reentry = 0; /* assume int $0x80 syscalls, crash otherwise */
	else
		sysenter_reentry = (long)&vdso[off];

	add_code_region(vdso, 0x1000, 0, 0, 0, 0); /* vdso */

	emu_start(prog.entry, prog.sp);

	sys_exit(1);
	return 1;
}
