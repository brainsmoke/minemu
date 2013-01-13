
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

#include "syscalls.h"
#include "error.h"
#include "load_elf.h"
#include "lib.h"
#include "error.h"
#include "mm.h"
#include "runtime.h"
#include "jit.h"
#include "jit_cache.h"
#include "codemap.h"
#include "sigwrap.h"
#include "options.h"

/* not called main() to avoid warnings about extra parameters :-(  */
int minemu_main(int argc, char *argv[], char *envp[], long auxv[])
{
	unsigned long pers = sys_personality(0xffffffff);
	

	if (ADDR_COMPAT_LAYOUT & ~pers)
	{
		sys_personality(ADDR_COMPAT_LAYOUT | pers);
		sys_execve("/proc/self/exe", argv, envp);
	}

	argv = parse_options(&argv[1]);

	init_minemu_mem(auxv, envp);
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

	jit(prog.entry);

	code_map_t *map = find_code_map(prog.entry);

	char *user_to_jit[map->len];
	char *jit_to_user[map->jit_len];
	memset(user_to_jit, 0, map->len*sizeof(long));
	memset(jit_to_user, 0, map->jit_len*sizeof(long));
	unsigned int i;
	char *rev_addr=NULL;
	char *op_start, *op_start_2;
	long op_len, op_len_2, j;

	for (i=0; i<map->len; i++)
	{
		char *jit_addr = jit_lookup_addr(&map->addr[i]);
		if ( jit_addr )
		{
			rev_addr = jit_rev_lookup_addr(jit_addr, &op_start, &op_len);
			if (rev_addr != &map->addr[i])
				debug("forward and reverse mapping do not match: reverse: %x expected: %x i=%x",
				      rev_addr, jit_addr, i);
			else for (j=1; j<op_len; j++)
			{
				rev_addr = jit_rev_lookup_addr(jit_addr+j, &op_start_2, &op_len_2);
				if (op_start_2 != op_start)
					debug("jit address changes in the middle of opcode %x to %x", op_start, op_start_2);
				if (op_len != op_len_2)
					debug("jit code size changes in the middle of opcode %x to %x", op_len, op_len_2);
			}
		}
	}

	sys_exit(0);
	return 0;
}
