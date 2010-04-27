
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <linux/ptrace.h>
#include <linux/unistd.h>

#include <sys/mman.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sched.h>

#include "errors.h"
#include "replay.h"
#include "debug.h"
#include "debug_syscalls.h"
#include "debug_wrap.h"
#include "trace.h"
#include "serialize.h"
#include "util.h"
#include "process.h"
#include "syscall_info.h"
#include "syscall_copy.h"
#include "errno.h"
#include "signal_info.h"
#include "call_data.h"
#include "emu_data.h"

static pid_t pidmap[100000];
static pid_t expect_pid = -1, new_pid = -1;
static int tracer_signal = 0, need_fork = 0;
static siginfo_t siginfo;

static in_stream_t *in;
static int steptrace;

static void handle_debug_data(trace_t *t)
{
	msg_type_t type;
	registers_t *regs;

	while ( (type = peek_type(in)) != EVENT_END && type != SIGNAL_EVENT )
		switch (type)
		{
			case REGISTERS:
				regs = read_registers(in);
				if (t->state == STEP)
					print_steptrace_debug(t, regs);
				else
					print_registers_if_diff(regs, &t->regs);
				break;
			default:
				fatal_error("unexpected message %d", type);
		}
}

static pid_t select_pid(void *data)
{
	if (expect_pid != -1)
	{
		pid_t pid = expect_pid;
		expect_pid = -1;
		return pid;
	}

	return pidmap[read_event_start(in)];
}

static void play_pre_call(trace_t *t, void *data)
{
	if (peek_type(in) == SIGNAL_EVENT)
	{
		undo_syscall(t);
		t->signal = read_signal_event(in, &siginfo);
		expect_pid = t->pid;
		tracer_signal = 1;
		handle_debug_data(t);
		read_event_end(in);
		return;
	}

	if (t->syscall == __NR_restart_syscall)
		t->syscall = emu_data(t)->restart_syscall;

	const syscall_info_t *info = syscall_info(t);
	int action = info->action;

	load_pre_call_data(t);

//	if (shadow_play_pre_call(in, t, NULL))
//		action = PASS;

	size_t argc = info->argc, orig_argc;
	long call, args[argc], orig_call, *orig_args;

	call = get_syscall(t);
	get_args(t, args, argc);
	if ( peek_type(in) == CALL )
	{
		orig_call = read_call(in, &orig_args, &orig_argc);
		if ( (call != orig_call) || (argc != orig_argc) ||
		     (bcmp(args, orig_args, argc)) )
			print_call(orig_call, orig_args, orig_argc);
	}

	compare_user_data(in, t);

	handle_debug_data(t);
	if ( (action & EMULATE) || (action == FAIL) ) 
		skip_syscall(t); /* for mmap2 we do everything during post_call */

	read_event_end(in);

	if ( ( t->syscall == __NR_clone ) ||
	     ( t->syscall == __NR_fork  ) ||
	     ( t->syscall == __NR_vfork ) )
	{
		/* peek whether the fork should be successful */
		expect_pid = t->pid;
		pid_t pid = read_event_start(in);
		if (pid != t->pid) /* fork was successful in record trace */
		{
			new_pid = pid;
			need_fork = 1;
		}
		else /* failure */
			skip_syscall(t);
	}
}

static void play_mmap(trace_t *t, void *start)
{
	long call, args[6];
	set_syscall(t, t->syscall);
	call = t->syscall;
	get_args(t, args, 6);
	args[0] = (long)start;
	long result;

	if ( args[0] < 0 && args[0] >= -516 )
	{
		set_result(t, (long)start);
	}
	else if ( (args[3] & MAP_ANONYMOUS) == 0 ) /* file backed */
	{
		long anon_mmap[] = /* replace by a non-file-backed map */
		{
			args[0], args[1], PROT_READ|PROT_WRITE|PROT_EXEC,
			(args[3]&~MAP_SHARED)|MAP_ANONYMOUS|MAP_PRIVATE, -1, args[5]
		};

		result = inject_syscall(t, call, anon_mmap, 6, NULL);
		set_result(t, result);

		if ( result < 0 && result >= -516 )
			return;

		unserialize_post_call_data(in, t);

		long mprotect_args[] = { result, args[1], args[2] };
		if ( (result = inject_syscall(t, __NR_mprotect,
		                              mprotect_args, 3, NULL)) < 0 )
			fatal_error("protecting mmap failed, %lx", result);
	}
	else
	{
		result = inject_syscall(t, call, args, 6, NULL);
		set_result(t, result);
		if ( start != (void *)result )
			fatal_error("replay mmap unsuccessful, result = %ld, needed = %ld",
			            result, start);
	}

	set_registers(t);
}

static void play_post_call(trace_t *t, void *data)
{
	const syscall_info_t *info = syscall_info(t);
	long result = read_result(in);

	if (need_fork)
		fatal_error("fork call failed in shadow trace");

	if (result == -ERESTART_RESTARTBLOCK)
		emu_data(t)->restart_syscall = t->syscall;

	if ( t->syscall == __NR_mmap2 )
		play_mmap(t, (void *)result );

	load_post_call_data(t);

	if ( (info->action & EMULATE) || (info->action == FAIL) ||
	     (info->action & COPY_RESULT) )
	{
		set_result(t, result);
		set_syscall(t, t->syscall);
		set_registers(t);

		unserialize_post_call_data(in, t);
	}
	else if (t->syscall == __NR_sigreturn)
	{
		registers_t *regs = read_registers(in);
		print_registers_if_diff(regs, &t->regs);
		t->regs = *regs;
		set_registers(t);
	}
	else
	{
		long r2 = get_result(t);
		if ( result != r2 )
			fprintf(stderr, "return %ld; / return %ld;\n", result, r2);

		compare_user_data(in, t);
	}

	if ( t->syscall == __NR_clone )
	{
		long flags = get_arg(t, 0);
		int *r_set_tid = (int *)get_arg(t, 3);
		int tid = get_result(t);

		if ( (tid>0) && (flags & CLONE_PARENT_SETTID) && ( r_set_tid != NULL ) )
			memstore(t->pid, (char *)&tid, (char *)r_set_tid, sizeof(int));
	}

	handle_debug_data(t);

	if ( peek_type(in) == SIGNAL_EVENT )
	{
		t->signal = read_signal_event(in, &siginfo);
		expect_pid = t->pid;
		tracer_signal = 1;
	}

	read_event_end(in);
}

static void play_signal(trace_t *t, void *data)
{
	get_siginfo(t->pid, &siginfo);

	if ( (t->signal == SIGSEGV) && program_counter_at_tsc(t) )
	{
		uint64_t ts = read_timestamp(in);
		handle_debug_data(t);
		read_event_end(in);
		emulate_tsc(t, ts);
		set_registers(t);
		t->signal = 0;
	}
	else if ( is_synchronous(&siginfo) )
	{
		if (read_signal_event(in, &siginfo) != t->signal)
			fatal_error("expected different signal");

		set_siginfo(t->pid, &siginfo);

		handle_debug_data(t);
		read_event_end(in);
	}
	else if (tracer_signal == 0)
	{
		t->signal = 0;
		expect_pid = t->pid;
	}
	else
		set_siginfo(t->pid, &siginfo);

	tracer_signal = 0;
}

static void play_start(trace_t *t, trace_t *parent, void *data)
{
	add_call_data(t);
	add_emu_data(t);

	if (steptrace)
		steptrace_process(t, 1);

	pidmap[new_pid] = t->pid;
	handle_debug_data(t);
	read_event_end(in);

	if ( t->syscall == __NR_clone )
	{
		long flags = get_arg(t, 0);
		int *r_set_tid = (int *)get_arg(t, 4);
		int tid = new_pid;

		if ( (flags & CLONE_CHILD_SETTID) && ( r_set_tid != NULL ) )
			memstore(t->pid, (char *)&tid, (char *)r_set_tid, sizeof(int));
	}

	need_fork = 0;
}

static void play_stop(trace_t *t, void *data)
{
	handle_debug_data(t);
	read_event_end(in);
	del_emu_data(t);
	del_call_data(t);
}

static void play_step(trace_t *t, void *data)
{
	handle_debug_data(t);
	read_event_end(in);
}

void replay(int read_fd, int verbosity)
{
	debug_init(stderr);

	init_pipe_channels();

	memset(pidmap, 0, sizeof(pid_t)*100000);

	tracer_plugin_t play = (tracer_plugin_t)
	{
		.pre_call = play_pre_call,
		.post_call = play_post_call,
		.signal = play_signal,
		.start = play_start,
		.stop = play_stop,
		.step = play_step,
		.pid_selector = select_pid,
	};

	in = open_in_stream(read_fd, 1, 1);

	tracer_plugin_t plug;

	if (verbosity)
		plug = debug_wrap(&play);
	else
		plug = play;

	steptrace = read_steptrace(in);
	char *exe = strdup(read_filepath(in));
	char **args = read_args(in);
	char **env = read_env(in);
	pid_t pid = run_traceable_env(exe, args, env, 1, 1);
	free(exe);
	free_string_array(args);
	free_string_array(env);

	new_pid = read_event_start(in);

	trace(pid, &plug);

	close_in_stream(in);
}

