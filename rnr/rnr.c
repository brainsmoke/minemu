
#include <sys/wait.h>

#include <fcntl.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "record.h"
/*
#include "shadow_dir.h"
*/
#include "replay.h"
#include "errors.h"

static char *infile = NULL, *outfile = NULL, debug_lvl = 1, verbosity = 0;

static char **parse_arguments(char **args)
{
	for (; *args && **args == '-'; args++)
	{
		if (strcmp(*args, "--") == 0)
		{
			args++;
			break;
		}
		else if ( (strcmp(*args, "-i"      ) == 0) ||
		          (strcmp(*args, "--infile") == 0) )
			infile = *++args;
		else if ( (strcmp(*args, "-o"       ) == 0) ||
		          (strcmp(*args, "--outfile") == 0) )
			outfile = *++args;
		else if (strcmp(*args, "-d") == 0)
			debug_lvl = atoi(*++args);
		else if (strcmp(*args, "-v") == 0)
			verbosity = 1;
		else if ( (strcmp(*args, "-s"      ) == 0) ||
		          (strcmp(*args, "--shadow") == 0) )
		{
			if ( !args[1] || !args[2] || add_shadow_dir(args[1], args[2])<0 )
			{
				fprintf(stderr, "error: --shadow takes two directories as "
				                "arguments\n");
				exit(EXIT_FAILURE);
			}
			args += 2;
		}
		else
		{
			fprintf(stderr, "error: unknown option: %s\n", *args);
			exit(EXIT_FAILURE);
		}

		if (*args == NULL)
			break;
	}

	if (infile && outfile)
	{
		fprintf(stderr, "error: --infile and --outfile are "
		                "mutually exclusive\n");
		exit(EXIT_FAILURE);
	}

	return args;
}

int main(int argc, char **argv)
{
	int filedes[2], in = -1, out = -1, cpid, status1 = 0, status2 = 0;

	

	char **child_args = parse_arguments(&argv[1]);

	if ( *child_args == NULL )
	{
		fprintf(stderr, "Usage: %s [options] command [arguments...]\n",
		                argv[0]);
		exit(EXIT_FAILURE);
	}

	if (infile)
	{
		if ( (in = open(infile, O_RDONLY)) == -1 )
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
	}
	else if (outfile)
	{
		if ( (out = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1 )
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		if ( pipe(filedes) == -1 )
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		in = filedes[0];
		out = filedes[1];
	}

	if (!infile)
	{
		cpid = fork();
		if (cpid < 0)
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (cpid == 0)
		{
			if (!outfile)
				close(in);

			fcntl(out, F_SETFD, FD_CLOEXEC);

			record(out, child_args, 1, debug_lvl, 0);
			close(out);
			exit(EXIT_SUCCESS);
		}
	}

	if (!outfile)
	{
		cpid = fork();
		if (cpid < 0)
		{
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (cpid == 0)
		{
			if (!infile)
				close(out);

			fcntl(in, F_SETFD, FD_CLOEXEC);

			replay(in, verbosity);
			close(in);
			exit(EXIT_SUCCESS);
		}
	}

	if (!infile)
		wait(&status1);

	if (!outfile)
		wait(&status2);

	exit( ( WEXITSTATUS(status1) | WIFSIGNALED(status1) |
	        WEXITSTATUS(status2) | WIFSIGNALED(status2) ) ? 1:0 );
}
