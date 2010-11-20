
#include <string.h>
#include <errno.h>

#include "exec.h"
#include "error.h"
#include "lib.h"
#include "syscalls.h"
#include "load_elf.h"
#include "load_script.h"
#include "jit_cache.h"
#include "taint_dump.h"

int can_load_binary(elf_prog_t *prog)
{
	int err = can_load_elf(prog);

	if (err)
		err = can_load_script(prog);

	return err;
}

int load_binary(elf_prog_t *prog)
{
	if ( load_elf(prog) == 0 )
		return 0;
	else
		return load_script(prog);
}

extern char *minemu_stack_bottom[];

long user_execve(char *filename, char *argv[], char *envp[])
{
	elf_prog_t prog =
	{
		.filename = filename,
		.argv     = argv,
		.envp     = envp,
	};
	long ret = can_load_binary(&prog), count = strings_count(argv);
	if (ret)
		return ret;


	char *cache_dir = get_jit_cache_dir();
	char *taint_dump_dir = get_taint_dump_dir();
	long args_start = 3 + (cache_dir ? 2 : 0) + (taint_dump_dir ? 2 : 0);

	/* abuse our minemu stack as allocated memory, our scratch stack is too small
	 * for exceptionally large argvs
	 */
	char **new_argv = &minemu_stack_bottom[- count - args_start - 1];
	new_argv[0] = argv[0];
	if (cache_dir)
	{
		new_argv[1] = "-cache";
		new_argv[2] = cache_dir;
	}
	if (taint_dump_dir)
	{
		new_argv[(cache_dir ? 2 : 0) + 1] = "-dump";
		new_argv[(cache_dir ? 2 : 0) + 2] = taint_dump_dir;
	}

	new_argv[args_start-2] = "--";
	new_argv[args_start-1] = filename;
	memcpy(&new_argv[args_start], argv, sizeof(char *)*(count+1));
	sys_execve("/proc/self/exe", new_argv, envp);
	die("user_execve failed");
	return 0xdeadbeef;
}

