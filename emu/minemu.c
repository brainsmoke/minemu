#include <sys/mman.h>
#include <linux/personality.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "error.h"
#include "exec.h"
#include "lib.h"
#include "error.h"
#include "mm.h"
#include "runtime.h"
#include "scratch.h"
#include "jit.h"
#include "jit_cache.h"
#include "codemap.h"
#include "sigwrap.h"
#include "taint_dump.h"

char *progname = NULL;

void usage(char *arg0)
{
	debug(
	"Usage: %s [options] [--] command arg1 ...\n"
	"\n"
	"Options:\n"
	"\n"
	"  -cache DIR          Cache jit code in DIR\n"
	"  -dump DIR           Dump taint info in DIR when a program gets killed\n"
	"                      because of a tainted jump\n"
	"  -exec EXECUTABLE    Use EXECUTABLE as executable filename, instead of\n"
	"                      doing path resolution on command\n",
	arg0
	);
	sys_exit(0);
}

char **parse_options(char **argv)
{
	char *arg0 = *argv;
	argv++;

	while (*argv)
	{
		if (**argv != '-')
			return argv;

		if ( strcmp(*argv, "--") == 0 )
			return argv+1;

		     if ( strcmp(*argv, "-cache") == 0 )
			set_jit_cache_dir(*++argv);
		else if ( strcmp(*argv, "-dump") == 0 )
			set_taint_dump_dir(*++argv);
		else if ( strcmp(*argv, "-exec") == 0 )
			progname = *++argv;
		else if ( strcmp(*argv, "-help") == 0 )
			usage(arg0);
		else
			die("unknown option: %s", *argv);

		argv++;
	}
	usage(arg0);
}

/* not called main() to avoid warnings about extra parameters :-(  */
int minemu_main(int argc, char *argv[], char **envp, long *auxv)
{
	unsigned long pers = sys_personality(0xffffffff);

	if (ADDR_COMPAT_LAYOUT & ~pers)
	{
		sys_personality(ADDR_COMPAT_LAYOUT | pers);
		sys_execve("/proc/self/exe", argv, envp);
	}

	argv = parse_options(argv);
	if ( (progname == NULL) && (argv[0][0] == '/') )
		progname = argv[0];

	init_minemu_mem(envp);
	sigwrap_init();
	jit_init();

	elf_prog_t prog =
	{
		.argv = argv,
		.envp = envp,
		.auxv = auxv,
		.task_size = USER_END,
		.stack_size = USER_STACK_SIZE,
	};

	int ret;

	if (progname)
	{
		prog.filename = progname;
		ret = load_binary(&prog);
	}
	else
	{
		ret = -ENOENT;
		char *path;
		if ( strchr(argv[0], '/') )
			path = getenve("PWD", envp);
		else
			path = getenve("PATH", envp);

		while ( (ret < 0) && (path != NULL) )
		{
			long len;
			char *next = strchr(path, ':');

			if (next)
			{
				len = (long)next-(long)path;
				next += 1;
			}
			else
				len = strlen(path);

			{
				char progname_buf[len + 1 + strlen(argv[0]) + 1];
				strncpy(progname_buf, path, len);
				progname_buf[len] = '/';
				progname_buf[len+1] = '\0';
				strcat(progname_buf, argv[0]);
				prog.filename = progname_buf;
				int ret_tmp = load_binary(&prog);
				if ( (ret != -EACCES) || (ret_tmp >= 0) )
					ret = ret_tmp;
			}
			path = next;
		}
	}

	if (ret < 0)
		die("unable to execute binary: %d", -ret);

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
