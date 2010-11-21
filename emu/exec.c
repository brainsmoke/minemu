
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
#include "jit_code.h"
#include "taint.h"

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
	long args_start = 4 + (cache_dir ? 2 : 0) +
	                      (taint_dump_dir ? 2 : 0) +
	                      (call_strategy!=PRESEED_ON_CALL ? 1 : 0) +
	                      (taint_flag == TAINT_OFF ? 1 : 0), i;

	/* abuse our minemu stack as allocated memory, our scratch stack is too small
	 * for exceptionally large argvs
	 */
	char **new_argv = &minemu_stack_bottom[- count - args_start - 1];
	new_argv[0] = argv[0];
	new_argv[1] = "-exec";
	new_argv[2] = filename;

	i = 3;
	if (cache_dir)
	{
		new_argv[i  ] = "-cache";
		new_argv[i+1] = cache_dir;
		i += 2;
	}
	if (taint_dump_dir)
	{
		new_argv[i  ] = "-dump";
		new_argv[i+1] = taint_dump_dir;
		i += 2;
	}
	if ( taint_flag == TAINT_OFF )
	{
		new_argv[i] = "-notaint";
		i++;
	}
	if ( call_strategy == PREFETCH_ON_CALL )
		new_argv[i] = "-prefetch";
	else if ( call_strategy == LAZY_CALL )
		new_argv[i] = "-lazy";

	new_argv[args_start-1] = "--";
	memcpy(&new_argv[args_start], argv, sizeof(char *)*(count+1));
	sys_execve("/proc/self/exe", new_argv, envp);
	die("user_execve failed");
	return 0xdeadbeef;
}

