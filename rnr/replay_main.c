#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "replay.h"
#include "errors.h"

static void print_help(char *progname)
{
	fprintf(stderr,
	        "Usage: %s [options] -i <filename>\n"
	        "       %s [options] -I <fd>\n\n",
	        progname, progname);
}

int main(int argc, char **argv)
{
	int verbosity = 0, in = -1, help = 0;
	char **argp;

	for (argp = &argv[1]; *argp && **argp == '-'; argp++)
	{
		if (strcmp(*argp, "--") == 0)
		{
			argp++;
			break;
		}
		else if ( (strcmp(*argp, "-i"       ) == 0) ||
		          (strcmp(*argp, "--in-file") == 0) )
		{
			if ( in != -1 )
				die("only one input method allowed");

			if ( (in = open(*++argp, O_RDONLY, 0644)) == -1 )
				die("open: %s", strerror(errno));
		}
		else if ( (strcmp(*argp, "-I"      ) == 0) ||
		          (strcmp(*argp, "--in-fd") == 0) )
		{
			if ( in != -1 )
				die("only one input method allowed");

			errno = 0;

			in = strtol(*++argp, (char **)NULL, 10);
			if ( !errno )
				fcntl(in, F_GETFD);

			if ( errno )
				die("wrong filedescriptor");
		}
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

	if ( (help) || (in == -1) || (*argp != NULL) )
	{
		print_help(argv[0]);
		exit(help ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	fcntl(in, F_SETFD, FD_CLOEXEC);
	replay(in, verbosity);
	close(in);
	exit(EXIT_SUCCESS);
}
