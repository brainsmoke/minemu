#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "record.h"
#include "errors.h"

static void print_help(char *progname)
{
	fprintf(stderr,
	        "Usage: %s [options] -o <filename> command [arguments...]\n"
	        "       %s [options] -O <fd> command [arguments...]\n\n",
	        progname, progname);
}

int main(int argc, char **argv)
{
	int debug_lvl = 1, verbosity = 0, out = -1, help = 0;
	char **argp;

	for (argp = &argv[1]; *argp && **argp == '-'; argp++)
	{
		if (strcmp(*argp, "--") == 0)
		{
			argp++;
			break;
		}
		else if ( (strcmp(*argp, "-o"        ) == 0) ||
		          (strcmp(*argp, "--out-file") == 0) )
		{
			if ( out != -1 )
				die("only one output method allowed");

			if ( (out = open(*++argp, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1 )
				die("open: %s", strerror(errno));
		}
		else if ( (strcmp(*argp, "-O"      ) == 0) ||
		          (strcmp(*argp, "--out-fd") == 0) )
		{
			if ( out != -1 )
				die("only one output method allowed");

			errno = 0;

			out = strtol(*++argp, (char **)NULL, 10);
			if ( !errno )
				fcntl(out, F_GETFD);

			if ( errno )
				die("wrong filedescriptor");
		}
		else if (strcmp(*argp, "-d") == 0)
			debug_lvl = atoi(*++argp);
		else if (strcmp(*argp, "-v") == 0)
			verbosity = 1;
		else if ( (strcmp(*argp, "-V"       ) == 0) ||
		          (strcmp(*argp, "--version") == 0) )
		{
			help = 1;
			break;
		}
		else
			die("unknown option: %s", *argp);
	}

	if ( (help) || (out == -1) || (*argp == NULL) )
	{
		print_help(argv[0]);
		exit(help ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	fcntl(out, F_SETFD, FD_CLOEXEC);
	record(out, argp, debug_lvl, verbosity);
	close(out);
	exit(EXIT_SUCCESS);
}
