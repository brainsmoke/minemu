
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

#include <sys/mman.h>
#include <linux/personality.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "error.h"
#include "exec.h"
#include "lib.h"
#include "mm.h"
#include "runtime.h"
#include "jit.h"
#include "codemap.h"
#include "sigwrap.h"
#include "options.h"
#include "exec_ctx.h"

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
	init_shield(TAINT_END);
	init_exec_ctx();
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

	init_vdso(get_aux(prog.auxv, AT_SYSINFO_EHDR));

	emu_start(prog.entry, prog.sp);

	sys_exit(1);
	return 1;
}
