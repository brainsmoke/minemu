#define _GNU_SOURCE 1

#include <fcntl.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/limits.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "record.h"
#include "trace.h"
#include "util.h"
#include "process.h"
#include "serialize.h"
#include "syscall_info.h"
#include "syscall_copy.h"
#include "syscall_validate.h"
#include "signal_info.h"
#include "signal_queue.h"
#include "emu_data.h"
#include "call_data.h"
#include "fd_data.h"
#include "debug_wrap.h"
#include "debug.h"

static int steptrace, debug;
static out_stream_t *out;

/* signal functions */

static pid_t expect_pid = -1;
static int post_call_signal = 0, dequeued_signal = 0;
static siginfo_t siginfo;

static void prepare_signal(trace_t *t)
{
	long signo;
	dequeue_signal(signal_queue(t), &signo, &siginfo);
	t->signal = signo;
	dequeued_signal = 1;
}

static void perform_signal(trace_t *t, int signo)
{
	get_siginfo(t->pid, &siginfo);
	write_event_start(out, t->pid);
	write_signal_event(out, t->signal, &siginfo);
	write_registers(out, &t->regs); /* */
	write_event_end(out);
	dequeued_signal = 0;
}

static void prepare_post_call_signal(trace_t *t)
{
	expect_pid = t->pid;
	steptrace_process(t, 1);
	post_call_signal = 1;
}

static void no_post_call_signal(trace_t *t)
{
	write_event_end(out);

	if (!steptrace)
		steptrace_process(t, 0);

	post_call_signal = 0;
}

static void perform_post_call_signal(trace_t *t, int signo)
{
	get_siginfo(t->pid, &siginfo);
	write_signal_event(out, signo, &siginfo);
	no_post_call_signal(t);
}

static pid_t signals_first(void *data)
{
	pid_t pid = expect_pid;
	expect_pid = -1;
	return pid;
}

/* altering calls */

void skip_call(trace_t *t, long retval)
{
	skip_syscall(t);
	emu_data_t *e = emu_data(t);
	e->retval = retval;
	e->call_skipped = 1;
}

void post_skip_call(trace_t *t)
{
	emu_data_t *e = emu_data(t);
	if (e->call_skipped)
	{
		set_result(t, e->retval);
		set_syscall(t, t->syscall);
		set_registers(t);
		e->call_skipped = 0;
	}
}

/*
 *
 */

static void record_pre_call(trace_t *t, void *_)
{
	/* see record_step(), this here is for the extremely unlikely
	 * event that there are two consecutive int $0x80's in the
	 * code and that a signal is expected after the first.
	 */
	if (post_call_signal)
	{
		fprintf(stderr, "%s:%d an extremely unlkely condition happened, "
		        "probably a bug\n", __FILE__, __LINE__);
		no_post_call_signal(t);
	}

	/* We have buffered one or more signals which we will deliver
	 * before we execute this syscall. We can safely synchronize here.
	 */
	if (signal_waiting(signal_queue(t)))
	{
		undo_syscall(t);
		prepare_signal(t);
		return;
	}

	if (t->syscall == __NR_restart_syscall)
		t->syscall = emu_data(t)->restart_syscall;

	const syscall_info_t *info = syscall_info(t);

	load_pre_call_data(t);

	write_event_start(out, t->pid);

	long error = 0;

	if ( uses_our_fd(t) )
		error = -EBADF;
	else if ( info->action == FAIL )
		error = -ENOSYS;

	if ( (t->syscall == __NR_clone) ||
	     (t->syscall == __NR_fork) ||
	     (t->syscall == __NR_vfork) )
		expect_pid = t->pid;

	if ( debug >= 1 )
	{
		long call, args[info->argc];
		call = get_syscall(t);
		get_args(t, args, info->argc);
		write_call(out, call, args, info->argc);
	}

	if ( debug >= 2 )
		write_registers(out, &t->regs);

	write_event_end(out);

	if (error)
		skip_call(t, error);
}

static void record_post_call(trace_t *t, void *_)
{
	post_skip_call(t); /* set the return value for calls we skipped */

	load_post_call_data(t);

	write_event_start(out, t->pid);

	long result = get_result(t);

	write_result(out, result);

	serialize_post_call_data(out, t);

	if ( (debug >= 2) || (t->syscall == __NR_sigreturn) )
		write_registers(out, &t->regs);

	if ( signal_pending(t->syscall, result) )
	{
		if (result == -ERESTART_RESTARTBLOCK)
			emu_data(t)->restart_syscall = t->syscall;

		if ( signal_waiting(signal_queue(t)) )
			prepare_signal(t);

		prepare_post_call_signal(t); /* don't finish event yet */
	}
	else
		write_event_end(out);
}

static void record_signal(trace_t *t, void *_)
{
	if (dequeued_signal)
		set_siginfo(t->pid, &siginfo);
	else
		get_siginfo(t->pid, &siginfo);

	if ( (t->signal == SIGSEGV) && program_counter_at_tsc(t) )
	{
		uint64_t ts = get_timestamp();
		write_event_start(out, t->pid);
		write_timestamp(out, ts);
		write_event_end(out);
		emulate_tsc(t, ts);
		set_registers(t);
		t->signal = NO_SIGNAL;
	}
	else if (post_call_signal)
	{
		perform_post_call_signal(t, t->signal);
	}
	else if ( dequeued_signal || is_synchronous(&siginfo) )
	{
		if (!steptrace)
			steptrace_process(t, 0);

		perform_signal(t, t->signal);
	}
	else
	{
		enqueue_signal(signal_queue(t), t->signal, &siginfo);
		t->signal = NO_SIGNAL;
	}
}

static void record_start(trace_t *t, trace_t *parent, void *_)
{
	add_call_data(t);
	add_signal_queue(t);
	add_emu_data(t);
	if (parent)
		copy_fd_data(t, parent);
	else
		new_fd_data(t);

	if (steptrace)
		steptrace_process(t, 1);

	write_event_start(out, t->pid);
	write_registers(out, &t->regs);
	write_event_end(out);
}

static void record_stop(trace_t *t, void *_)
{
	if (post_call_signal)
		no_post_call_signal(t);

	write_event_start(out, t->pid);

	if ( debug >= 2 )
		write_registers(out, &t->regs);

	write_event_end(out);

	del_emu_data(t);
	del_signal_queue(t);
	del_call_data(t);
	del_fd_data(t);
}

static void record_step(trace_t *t, void *_)
{
	/* if we expect a signal just after a syscall, we turn on
	 * steptracing briefly to see if a signal event is the next
	 * thing happening to a process. If it comes to this, we know
	 * it's not.
	 */
	if (post_call_signal)
		no_post_call_signal(t);

	if (!steptrace)
		return;

	/* only for debug level 5, VERY slow */
	write_event_start(out, t->pid);
	write_registers(out, &t->regs);
	write_event_end(out);
}

void record(int write_fd, char **args, int debug_lvl, int verbosity)
{
	debug_init(stderr);

	memset(&siginfo, 0, sizeof(siginfo_t));

	init_pipe_channels();

	out = open_out_stream(write_fd, 1);

	tracer_plugin_t rec =
	{
		.pre_call = record_pre_call,
		.post_call = record_post_call,
		.signal = record_signal,
		.start = record_start,
		.stop = record_stop,
		.step = record_step,
		.pid_selector = signals_first,
	};

	tracer_plugin_t plug;

	if (verbosity)
		plug = debug_wrap(&rec);
	else
		plug = rec;

	debug = debug_lvl;
	steptrace = debug_lvl >= 5 ? 1:0;

	write_steptrace(out, steptrace);

	pid_t pid = run_traceable(args[0], args, 1, 1);

	char *exe = get_proc_exe(pid);
	write_filepath(out, exe);
	free(exe);

	write_args(out, args);
	write_env(out, environ);

	trace(pid, &plug);

	close_out_stream(out);
}

