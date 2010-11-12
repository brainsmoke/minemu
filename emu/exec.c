
#include <string.h>

#include "exec.h"
#include "error.h"
#include "lib.h"
#include "syscalls.h"
#include "load_elf.h"
#include "jit_cache.h"

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


	char *cache_dir = get_jit_cache_dir();
	long args_start = 3 + (cache_dir ? 2 : 0);

	/* abuse our temu stack as allocated memory, our scratch stack is too small
	 * for exceptionally large argvs
	 */
	char **new_argv = &temu_stack_bottom[- count - args_start - 1];
	new_argv[0] = argv[0];
	if (cache_dir)
	{
		new_argv[1] = "-cache";
		new_argv[2] = cache_dir;
	}
	new_argv[args_start-2] = "--";
	new_argv[args_start-1] = filename;
	memcpy(&new_argv[args_start], argv, sizeof(char *)*(count+1));
	sys_execve("/proc/self/exe", new_argv, envp);
	die("user_execve failed");
	return 0xdeadbeef;
}

