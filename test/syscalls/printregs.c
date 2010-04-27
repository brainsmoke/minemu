
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <linux/ptrace.h>
#include <sys/personality.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "errors.h"
#include "debug.h"
#include "debug_syscalls.h"
#include "trace.h"
#include "util.h"
#include "process.h"

static const char *c = "\033[0;31m", *n = "\033[m";

int step;

static void print_pre_call(trace_t *t, void *data)
{
	print_trace_call(t);
	if (step)
		print_trace_diff(t, (trace_t*)t->data);
	else
		print_trace(t);
	*(trace_t*)t->data = *t;
	fflush(stdout);
}

static void print_post_call(trace_t *t, void *data)
{
	print_trace_return(t);
	printf("(from %s)\n", syscall_name(get_syscall(t)));
	print_trace_diff(t, (trace_t*)t->data);
	if (step)
		*(trace_t*)t->data = *t;
	fflush(stdout);
}

static void print_signal(trace_t *t, void *data)
{
	siginfo_t info;
	memset(&info, 0, sizeof(info));

	printf("%sSIGNAL%s %s\n",c,n, signal_name(t->signal));
	get_siginfo(t->pid, &info);
	printhex(&info, sizeof(info));
	if (step)
	{
		print_trace_diff(t, (trace_t*)t->data);
		*(trace_t*)t->data = *t;
	}
	else
		print_trace(t);
	fflush(stdout);
}

static void print_start(trace_t *t, trace_t *parent, void *data)
{
	if (step)
		steptrace_process(t, 1);
	t->data = try_malloc(sizeof(trace_t));
	printf("%sSTART%s\n",c,n);
	print_trace(t);
	if (step)
		*(trace_t*)t->data = *t;
	fflush(stdout);
}

static void print_stop(trace_t *t, void *data)
{
	free(t->data);
	printf("%sSTOP%s\n",c,n);
	if (step)
		print_trace_diff(t, (trace_t*)t->data);
	else
		print_trace(t);
	fflush(stdout);
}

static void print_step(trace_t *t, void *data)
{
	printf("%sSTEP%s\n",c,n);
	print_trace_diff(t, (trace_t*)t->data);
	*(trace_t*)t->data = *t;
	fflush(stdout);
}

static void print_exec(trace_t *t, void *data)
{
	printf("%sEXEC%s\n",c,n);
	print_trace_diff(t, (trace_t*)t->data);
	*(trace_t*)t->data = *t;
	fflush(stdout);
}

int main(int argc, char **argv)
{
	debug_init(stdout);

	step = (strcmp(argv[1], "-step") == 0) ? 1:0;

	tracer_plugin_t plug = (tracer_plugin_t)
	{
		.pre_call = print_pre_call,
		.post_call = print_post_call,
		.signal = print_signal,
		.start = print_start,
		.stop = print_stop,
		.step = print_step,
		.exec = print_exec,
		.pid_selector = any_pid, /* always returns -1 */
		.data = NULL,
	};

	trace(run_traceable(argv[1+step], &argv[1+step], 1, 0), &plug);

	exit(EXIT_SUCCESS);
}

