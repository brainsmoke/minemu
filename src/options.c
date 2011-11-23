
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

#include "syscalls.h"
#include "error.h"
#include "lib.h"
#include "jit_cache.h"
#include "taint_dump.h"
#include "jit_code.h"
#include "taint.h"
#include "sigwrap.h"
#include "threads.h"

char *progname = NULL;

static void load_sigset(char *sigset_buf)
{
	if (strlen(sigset_buf) < 16)
		return;

	kernel_sigset_t mask;
	((unsigned long *)&mask)[0] = hexread(sigset_buf);
	((unsigned long *)&mask)[1] = hexread(&sigset_buf[8]);
	get_thread_ctx()->old_sigset = mask;
}

static void save_sigset(char *sigset_buf)
{
	sigset_buf[0] = '\0';
	kernel_sigset_t mask = get_thread_ctx()->old_sigset;
	hexcat(sigset_buf,  ((unsigned long *)&mask)[0]);
	hexcat(sigset_buf,  ((unsigned long *)&mask)[1]);
}

void usage(char *arg0)
{
	debug(
	"Usage: %s [options] [--] command arg1 ...\n"
	"\n"
	"Options:\n"
	"\n"
	"  -cache DIR          Cache jit code in DIR.\n"
	"  -dump DIR           Dump taint info in DIR when a program gets\n"
	"                      terminated because of a tainted jump.\n"
	"  -exec EXECUTABLE    Use EXECUTABLE as executable filename, instead of\n"
	"                      doing path resolution on command.\n"
	"\n"
	"  -dumponexit         Also dump taint info when a program exits normally\n"
	"  -nodumponexit       Don't dump taint info when a program exits normally (default)\n"
	"  -dumpall            Dump all pages\n"
	"  -dumptainted        Only dump tainted pages (default)\n"
	"\n"
	"  -preseed            For call instructions: seed the emulator's jump cache\n"
	"                      with the return address of the call. (default)\n"
	"  -prefetch           For call instructions: prefetch the emulator's jump cache\n"
	"                      in anticipation of the return.\n"
	"  -lazy               Do not seed or prefetch caches for call instructions.\n"
	"\n"
	"  -taint              Turn on tainting. (default)\n"
	"  -notaint            Turn off tainting.\n"
	"\n"
	"  -trackfiles         Taint files which are not in known executable locations\n"
	"  -trusteddirs        Trust files from these locations (implies -trackfiles)\n"
	"                      default dirs: %s\n"
	"\n"
	"  -help               Show this message and exit.\n"
	"  -version            Print version number and exit.\n",
	arg0
	);
	sys_exit(0);
}

void version(void)
{
	debug("minemu version 0.6");
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
		else if ( strcmp(*argv, "-sigmask") == 0 )
			load_sigset(*++argv);
		else if ( strcmp(*argv, "-help") == 0 )
			usage(arg0);
		else if ( strcmp(*argv, "-version") == 0 ||
		          strcmp(*argv, "-v") == 0 )
			version();
		else if ( strcmp(*argv, "-preseed") == 0 )
			call_strategy = PRESEED_ON_CALL;
		else if ( strcmp(*argv, "-prefetch") == 0 )
			call_strategy = PREFETCH_ON_CALL;
		else if ( strcmp(*argv, "-lazy") == 0 )
			call_strategy = LAZY_CALL;
		else if ( strcmp(*argv, "-taint") == 0 )
			taint_flag = TAINT_ON;
		else if ( strcmp(*argv, "-notaint") == 0 )
			taint_flag = TAINT_OFF;
		else if ( strcmp(*argv, "-dumponexit") == 0 )
			dump_on_exit = 1;
		else if ( strcmp(*argv, "-nodumponexit") == 0 )
			dump_on_exit = 0;
		else if ( strcmp(*argv, "-dumpall") == 0 )
			dump_all = 1;
		else if ( strcmp(*argv, "-dumptainted") == 0 )
			dump_all = 0;
		else if ( strcmp(*argv, "-trackfiles") == 0 && !trusted_dirs )
			trusted_dirs = trusted_dirs_default;
		else if ( strcmp(*argv, "-trusteddirs") == 0 )
			set_trusted_dirs(*++argv);
		else
			die("unknown option: %s", *argv);

		argv++;
	}
	usage(arg0);
}

long option_args_count(void)
{
	return 2 + /* -exec ... */ 2 + /* -sigmask ... */
	       (get_jit_cache_dir()                   ? 2 : 0) +
	       (get_taint_dump_dir()                  ? 2 : 0) +
	       (dump_on_exit                          ? 1 : 0) +
	       (dump_all                              ? 1 : 0) +
	       (call_strategy != PRESEED_ON_CALL      ? 1 : 0) +
	       (taint_flag == TAINT_OFF               ? 1 : 0) +
	       (trusted_dirs                          ? 1 : 0) +
	       (trusted_dirs != trusted_dirs_default  ? 1 : 0) +
	       1; /* -- */
}

char **option_args_setup(char **argv, char *filename, char *sigset_buf)
{
	char *taint_dump_dir = get_taint_dump_dir();
	char *cache_dir = get_jit_cache_dir();

	long i=0;

	argv[i  ] = "-exec";
	argv[i+1] = filename;
	i += 2;

	save_sigset(sigset_buf);
	argv[i  ] = "-sigmask";
	argv[i+1] = sigset_buf;
	i += 2;

	if (cache_dir)
	{
		argv[i  ] = "-cache";
		argv[i+1] = cache_dir;
		i += 2;
	}
	if (taint_dump_dir)
	{
		argv[i  ] = "-dump";
		argv[i+1] = taint_dump_dir;
		i += 2;
	}
	if ( taint_flag == TAINT_OFF )
	{
		argv[i] = "-notaint";
		i++;
	}
	if ( dump_on_exit )
	{
		argv[i] = "-dumponexit";
		i++;
	}
	if ( dump_all )
	{
		argv[i] = "-dumpall";
		i++;
	}
	if ( call_strategy == PREFETCH_ON_CALL )
	{
		argv[i] = "-prefetch";
		i++;
	}
	if ( call_strategy == LAZY_CALL )
	{
		argv[i] = "-lazy";
		i++;
	}
	if (trusted_dirs == trusted_dirs_default)
	{
		argv[i] = "-trackfiles";
		i++;
	}
	else if (trusted_dirs)
	{
		argv[i] = "-trusteddirs";
		i++;
		argv[i] = trusted_dirs;
		i++;
	}

	argv[i] = "--";
	i++;

	return &argv[i];
}

