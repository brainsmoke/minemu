#include <string.h>

#include "syscalls.h"
#include "error.h"
#include "lib.h"
#include "jit_cache.h"
#include "taint_dump.h"
#include "jit_code.h"
#include "taint.h"

char *progname = NULL;

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
	"  -dumponexit         Also dump taint info when a program exits normally\n"
	"  -nodumponexit       Don't dump taint info when a program exits normally (default)\n"
	"  -exec EXECUTABLE    Use EXECUTABLE as executable filename, instead of\n"
	"                      doing path resolution on command.\n"
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
	"  -help               Show this message and exit.\n"
	"  -version            Print version number and exit.\n",
	arg0
	);
	sys_exit(0);
}

void version(void)
{
	debug("minemu version 0.1");
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
		else
			die("unknown option: %s", *argv);

		argv++;
	}
	usage(arg0);
}

long option_args_count(void)
{
	return 2 + /* -exec ... */
	       (get_jit_cache_dir()              ? 2 : 0) +
	       (get_taint_dump_dir()             ? 2 : 0) +
	       (dump_on_exit                     ? 1 : 0) +
	       (call_strategy != PRESEED_ON_CALL ? 1 : 0) +
	       (taint_flag == TAINT_OFF          ? 1 : 0) +
	       1; /* -- */
}

char **option_args_setup(char **argv, char *filename)
{
	char *taint_dump_dir = get_taint_dump_dir();
	char *cache_dir = get_jit_cache_dir();

	long i=0;

	argv[i  ] = "-exec";
	argv[i+1] = filename;
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

	argv[i] = "--";
	i++;

	return &argv[i];
}

