
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


#include <linux/binfmts.h>
#include <errno.h>
#include <string.h>

#include "load_script.h"


#include "error.h"
#include "syscalls.h"
#include "lib.h"

/* relocate the stack */

static int try_load_script(elf_prog_t *prog, long bailout)
{
	int fd, err, size, i, last = BINPRM_BUF_SIZE-2;
	char buf[BINPRM_BUF_SIZE]; buf[BINPRM_BUF_SIZE-1] = '\0';
	char *interp = NULL, *interp_arg = NULL, *script_name;
	int interp_read = 0;

	if ( (fd = open_exec(prog->filename)) < 0 )
		return fd;

	size = read_at(fd, 0, buf, BINPRM_BUF_SIZE);
	sys_close(fd);

	if ( size < 0 )
		return size;

	if ( memcmp(buf, "#!", 2) )
		return -ENOEXEC;

	for (i=2; i<size; i++)
	{
		if ( buf[i] == '\n' || buf[i] == '\0' )
		{
			break;
		}
		if ( buf[i] == ' ' || buf[i] == '\t' )
		{
			if ( interp != NULL && interp_arg == NULL )
			{
				buf[i] = '\0';
				interp_read = 1;
			}
		}
		else
		{
			if ( interp == NULL )
				interp = &buf[i];
			else if ( interp_read && interp_arg == NULL)
				interp_arg = &buf[i];

			last = i;
		}
	}

	if ( !interp )
		return -ENOEXEC;

	buf[last+1] = '\0';

	script_name = prog->filename;
	prog->filename = interp;

	if ( bailout )
	{
		err = can_load_elf(prog);
		prog->filename = script_name;
		return err;
	}

	long argc = strings_count(prog->argv)+1;
	char *argv[argc+2], **old_argv = prog->argv;
	argv[0] = interp;
	if (interp_arg)
	{
		argv[1] = interp_arg;
		argc++;
	}

	argv[interp_arg ? 2:1] = script_name;

	memcpy(&argv[interp_arg ? 3:2], &prog->argv[1], (argc)*sizeof(char *));
	prog->argv = argv;

	err = load_elf(prog);

	/* loading binary failed */

	prog->argv = old_argv;
	prog->filename = script_name;

	return err;
}

int can_load_script(elf_prog_t *prog)
{
	return try_load_script(prog, 1);
}

int load_script(elf_prog_t *prog)
{
	return try_load_script(prog, 0);
}

