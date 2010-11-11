
#include <string.h>

#include "exec.h"
#include "error.h"
#include "lib.h"
#include "syscalls.h"
#include "load_elf.h"

static long argv_count(char *argv[])
{
	long i;

	for (i=0; argv[i]; i++);

	return i;
}

extern char *temu_stack_bottom[];

long user_execve(char *filename, char *argv[], char *envp[])
{
	elf_prog_t prog =
	{
		.filename = filename,
		.argv     = argv,
		.envp     = envp,
	};
	long ret = can_load_elf(&prog), count = argv_count(argv);
	if (ret)
		return ret;

	/* abuse our temu stack as allocated memory, our scratch stack is too small
	 * for exceptionally large argvs
	 */
	char **new_argv = &temu_stack_bottom[- count - 4];
	new_argv[0] = argv[0];
	new_argv[1] = "--";
	new_argv[2] = filename;
	memcpy(&new_argv[3], argv, sizeof(char *)*(count+1));
	sys_execve("/proc/self/exe", new_argv, envp);
	die("user_execve failed");
	return 0xdeadbeef;
}

