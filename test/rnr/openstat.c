#define _LARGEFILE64_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>

#include <linux/unistd.h>

#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "trace.h"
#include "util.h"
#include "process.h"

static void stat_on_open(trace_t *t, void *data)
{

	if ( get_syscall(t) == __NR_open )
	{
		long fd = get_result(t), result=-1;
		char filename[4096]; filename[4095] = 0;
		struct stat64 s;

		memloadstr(t->pid, filename, (void *)get_arg(t, 0), 4095);
		fprintf(stderr, "open(\"%s\", %ld, %ld) => %ld\n",
		                filename, get_arg(t, 1), get_arg(t, 2), fd);

		if ( fd >= 0 )
			result = inject_fstat64(t, fd, &s, NULL); /* ignores signals */

		if ( result < 0 )
			fprintf(stderr, "error: fstat failed\n");
		else
			print_stat(&s);

		fflush(stderr);
	}
}

int main(int argc, char **argv)
{
	tracer_plugin_t plug = (tracer_plugin_t)
	{
		.post_call = stat_on_open,
		.pid_selector = any_pid,
	};

	debug_init(stderr);

	init_pipe_channels();

	trace(run_traceable(argv[1], &argv[1], 1, 0), &plug);

	exit(EXIT_SUCCESS);
}

