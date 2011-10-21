
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <errno.h>

#include "exec.h"
#include "error.h"
#include "syscalls.h"
#include "load_elf.h"
#include "load_script.h"
#include "options.h"

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

	/* abuse our minemu stack as allocated memory, our scratch stack is too small
	 * for exceptionally large argvs
	 */
	char **new_argv = &minemu_stack_bottom[- count - option_args_count() - 2], **user_argv;
	new_argv[0] = argv[0];
	user_argv = option_args_setup(&new_argv[1], filename);
	memcpy(user_argv, argv, sizeof(char *)*(count+1));
	sys_execve("/proc/self/exe", new_argv, envp);
	die("user_execve failed");
	return 0xdeadbeef;
}

