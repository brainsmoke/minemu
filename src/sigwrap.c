
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 * Copyright 2011 Vrije Universiteit Amsterdam
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


#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "segments.h"
#include "sigwrap.h"
#include "runtime.h"
#include "error.h"
#include "mm.h"
#include "lib.h"
#include "jit.h"
#include "jit_code.h"
#include "jit_fragment.h"
#include "debug.h"
#include "taint.h"
#include "taint_dump.h"
#include "threads.h"

/*
 * wrapper around signals, preventing signals being delivered on the
 * runtime stack, or otherwise within an emulated opcode which would be
 * atomic when executed natively
 */

/* for the emulation of some some non-blocking syscalls, we block all signals
 *
 */
int try_block_signals(void)
{
	kernel_sigset_t blockall;
	memset(&blockall, 0xff, sizeof(blockall));
	return !syscall_intr(__NR_rt_sigprocmask, SIG_BLOCK,
	                     (long)&blockall,
	                     (long)&get_thread_ctx()->old_sigset,
	                     sizeof(kernel_sigset_t),0,0);
}

int block_signals(void)
{
	kernel_sigset_t blockall;
	memset(&blockall, 0xff, sizeof(blockall));
	return syscall4(__NR_rt_sigprocmask, SIG_BLOCK,
	                     (long)&blockall,
	                     (long)&get_thread_ctx()->old_sigset,
	                     sizeof(kernel_sigset_t));
}

void unblock_signals(void)
{
	syscall_intr(__NR_rt_sigprocmask, SIG_SETMASK,
	             (long)&get_thread_ctx()->old_sigset,
	             (long)NULL,
	             sizeof(kernel_sigset_t),0,0);
}

#ifndef SA_RESTORER
#define SA_RESTORER        (0x04000000)
#endif

static unsigned long fpstate_size = sizeof(struct _fpstate);

void *get_sigframe_addr(struct kernel_sigaction *action, struct sigcontext *context, long size)
{
    unsigned long sp = context->esp;
	thread_ctx_t *local_ctx = get_thread_ctx();

	sp &= ~0x3f; /* 64 byte align */

	if ( contains(local_ctx->altstack.ss_sp, local_ctx->altstack.ss_size, (char*)sp) &&
	    !contains(local_ctx->altstack.ss_sp, local_ctx->altstack.ss_size, (char*)(sp-size)) )
		/* This will segfault/terminate the program, just as the normal program would,
		 * mirrors kernel code and it's simple
		 */
		return (void *) -1L;

	if ( (action->flags & SA_ONSTACK) &&
	     (local_ctx->altstack.ss_size > 0) &&
	     (!contains(local_ctx->altstack.ss_sp, local_ctx->altstack.ss_size, (char *)sp)) )
		sp = (unsigned long) local_ctx->altstack.ss_sp + local_ctx->altstack.ss_size;

	/* assert that we restored to the shielded state */
	if ((context->ss & 0xffff) != shield_segment)
		die("wrong user segment");

	else if ( ((context->ss & 0xffff) != shield_segment) &&
	         !(action->flags & SA_RESTORER) &&
	          (action->restorer) )
		sp = (unsigned long) action->restorer;

	sp -= size;
	return (void*)sp;
}

static void sigframe_patch_pointers(struct kernel_sigframe *new,
                                    struct kernel_sigframe *old)
{
	*(long*)&new->sc.fpstate += (long)new - (long)old;

	if (old->pretcode == &new->retcode[0])
		new->pretcode = &new->retcode[0];
}

static void rt_sigframe_patch_pointers(struct kernel_rt_sigframe *new,
                                       struct kernel_rt_sigframe *old)
{
	*(long*)&new->uc.uc_mcontext.fpstate += (long)new - (long)old;

	if (old->pretcode == &old->retcode[0])
		new->pretcode = &new->retcode[0];

	new->pinfo = &new->info;
	new->puc = &new->uc;
	new->uc.uc_stack = get_thread_ctx()->altstack;
}

static void *copy_frame_to_user(void *frame, struct kernel_sigaction *action,
                                             struct sigcontext *context)
{
	thread_ctx_t *local_ctx = get_thread_ctx();
	char *sigstack_end = (char *)local_ctx->sigwrap_stack + sizeof(local_ctx->sigwrap_stack);

	unsigned long            size = (unsigned long)sigstack_end - (unsigned long)frame;
	unsigned long fpstate_minsize = (unsigned long)sigstack_end - (unsigned long)context->fpstate;

	if (fpstate_size < fpstate_minsize)
		fpstate_size = fpstate_minsize;

	void *copy = get_sigframe_addr(action, context, size);

	memcpy(copy, frame, size);
	taint_mem(copy, size, TAINT_CLEAR);
	return copy;
}

static struct kernel_rt_sigframe *copy_rt_sigframe_to_user(struct kernel_rt_sigframe *frame,
                                                           struct kernel_sigaction *action)
{
	struct kernel_rt_sigframe *copy = copy_frame_to_user(frame, action, &frame->uc.uc_mcontext);
	rt_sigframe_patch_pointers(copy, frame);

	if (contains((char *)vdso_orig, 0x1000, copy->pretcode))
		copy->pretcode += vdso - vdso_orig;

	if (action->flags & SA_RESTORER)
		copy->pretcode = (char *)(long)action->restorer;

	return copy;
}

static struct kernel_sigframe *copy_sigframe_to_user(struct kernel_sigframe *frame,
                                                     struct kernel_sigaction *action)
{
	struct kernel_sigframe *copy = copy_frame_to_user(frame, action, &frame->sc);
	sigframe_patch_pointers(copy, frame);

	if (contains((char *)vdso_orig, 0x1000, copy->pretcode))
		copy->pretcode += vdso - vdso_orig;

	if (action->flags & SA_RESTORER)
		copy->pretcode = (char *)(long)action->restorer;

	return copy;
}

static void dump_on_error(int sig, struct sigcontext *context)
{
	if ( (sig == SIGSEGV) || (sig == SIGILL) || (sig == SIGFPE) )
	{
		sys_read(-1, context->cr2, -1);
		sys_read(-1, context->eip, -1);

		if (!get_taint_dump_dir())
			return;

		get_thread_ctx()->user_eip = (long)jit_rev_lookup_addr((char *)context->eip, NULL, NULL);
		long regs[] = { context->eax, context->ecx, context->edx, context->ebx,
		                context->esp, context->ebp, context->esi, context->edi, };
		do_taint_dump(regs);
		if (sig == SIGSEGV)
			asm("movl $0, 0"::);
		else
			asm("ud2"::);
	}
}

static void sigwrap_handler(int sig, siginfo_t *info, void *_)
{
	thread_ctx_t *local_ctx = get_thread_ctx();
	long *return_stackp = (long *)(((long)&sig)-4);
	struct kernel_rt_sigframe *rt_sigframe = (struct kernel_rt_sigframe *) return_stackp;
	struct kernel_sigframe    *sigframe    = (struct kernel_sigframe *)    return_stackp;
	struct sigcontext *context;
	unsigned long *sigmask, *extramask;

	siglock(local_ctx);
	struct kernel_sigaction action = local_ctx->sighandler->sigaction_list[sig];
	if ( action.flags & SA_ONESHOT )
		memset(&local_ctx->sighandler->sigaction_list[sig], 0, sizeof(struct kernel_sigaction));

	sigunlock(local_ctx);

	if ( (sig < 0) || (sig >= KERNEL_NSIG) )
		die("bad signo. %d", sig);

	if ( action.flags & SA_SIGINFO )
		context = &rt_sigframe->uc.uc_mcontext;
	else
		context = &sigframe->sc;

	dump_on_error(sig, context);

	/* original code address */
	context->eip = (long)finish_instruction(context);

	/* Most evil hack ever! We 'deliver' the user's signal by modifying our own sigframe
	 * to match the user process' state at signal delivery, and call sigreturn.
	 */
	if ( action.flags & SA_SIGINFO )
	{
		sigmask = &rt_sigframe->uc.uc_sigmask.bitmask[0];
		extramask = &rt_sigframe->uc.uc_sigmask.bitmask[1];
		rt_sigframe = copy_rt_sigframe_to_user(rt_sigframe, &action);
		context->esp = (long)rt_sigframe;
		context->ecx = (long)&rt_sigframe->uc;
		context->edx = (long)&rt_sigframe->info;
		*return_stackp = (long)do_rt_sigreturn;
	}
	else
	{
		sigmask = &sigframe->sc.oldmask;
		extramask = &sigframe->extramask[0];
		sigframe = copy_sigframe_to_user(sigframe, &action);
		context->esp = (long)sigframe;
		context->ecx = 0;
		context->edx = 0;
		*return_stackp = (long)do_sigreturn;
	}

	/* registers */
	context->ds =
	context->es =
	context->ss = shield_segment;
	context->cs = code_segment;

	local_ctx->user_eip = (long)action.handler;   /* jump into jit (or in this case runtime) */
	context->eip = (long)state_restore;           /* code, not user code                     */
	context->eax = sig;

	/* signal state */
	*sigmask   |= action.mask.bitmask[0];
	*extramask |= action.mask.bitmask[1];
	if ( !(action.flags & SA_NODEFER) )
	{
		if (sig <= 32)
			*sigmask |= 1UL << (sig-1);
		else
			*extramask |= 1UL << (sig-33);
	}

	/* return into (rt_)sigreturn */
}

void user_sigreturn(void)
{
	thread_ctx_t *local_ctx = get_thread_ctx();

	long *esp = (long *)local_ctx->user_esp;
	struct kernel_sigframe *sigframe = (struct kernel_sigframe *)&esp[-2];
	struct kernel_sigframe frame = *sigframe;
	struct sigcontext *context = &frame.sc;
	
	local_ctx->user_eip = context->eip;            /* jump into jit code, */
	context->eip = (long)state_restore;            /* not user code       */
	load_sigframe(&frame);
}

void user_rt_sigreturn(void)
{
	thread_ctx_t *local_ctx = get_thread_ctx();

	long *esp = (long *)local_ctx->user_esp;
	struct kernel_rt_sigframe *rt_sigframe = (struct kernel_rt_sigframe *)&esp[-1];
	struct kernel_rt_sigframe frame = *rt_sigframe;
	struct sigcontext *context = &frame.uc.uc_mcontext;
	local_ctx->user_eip = context->eip;            /* jump into jit code, */
	context->eip = (long)state_restore;            /* not user code       */
	frame.uc.uc_stack = (stack_t)
	{
		.ss_sp = local_ctx->sigwrap_stack,
		.ss_flags = 0,
		.ss_size = sizeof( local_ctx->sigwrap_stack )
	};
	load_rt_sigframe(&frame);
}

void altstack_setup(void)
{
	thread_ctx_t *local_ctx = get_thread_ctx();
	long ret;

	stack_t sigwrap_altstack =
	{
		.ss_sp = local_ctx->sigwrap_stack,
		.ss_flags = 0,
		.ss_size = sizeof( local_ctx->sigwrap_stack )
	};

	if ( (ret=sys_sigaltstack(&sigwrap_altstack, &local_ctx->altstack)) )
		die("altstack_setup: sigaltstack failed: %d", ret);
}

static void altstack_restore(void)
{
	long ret;
	if ( (ret=sys_sigaltstack(&get_thread_ctx()->altstack, NULL)) )
		die("altstack_restore: sigaltstack failed: %d", ret);
}

/* should be called at the start
 *
 */
void sigwrap_init(void)
{
	memset(get_thread_ctx()->sighandler->sigaction_list, 0, KERNEL_NSIG*sizeof(struct kernel_sigaction));

	struct kernel_sigaction act =
	{
		.handler = sigwrap_handler,
		.flags = SA_ONSTACK|SA_SIGINFO,
	};

	memset(&act.mask, 0xff, sizeof(act.mask));
	if (get_taint_dump_dir())
	{
		sys_rt_sigaction(SIGSEGV, &act, NULL, sizeof(act.mask));
		sys_rt_sigaction(SIGILL, &act, NULL, sizeof(act.mask));
	}
}

/* the emulator blocks signals */
/* TODO pointer check, ON_STACK flag */
long user_sigaltstack(const stack_t *ss, stack_t *oss)
{
	long ret;
	unprotect_ctx();
	altstack_restore();
	ret = sys_sigaltstack(ss, oss);
	altstack_setup();
	protect_ctx();
	return ret;
}

unsigned long user_signal(int sig, void (*handler) (int, siginfo_t *, void *))
{
	struct kernel_sigaction rt_act, rt_oact;
	unsigned long ret;

	rt_act = (struct kernel_sigaction)
	{
		.handler = handler,
		.flags = SA_ONESHOT|SA_NOMASK
	};

	ret = user_rt_sigaction(sig, &rt_act, &rt_oact, sizeof(sigset_t));

	return ret ? ret : (unsigned long)rt_oact.handler;
}

long user_sigaction(int sig, const struct kernel_old_sigaction *act,
                                   struct kernel_old_sigaction *oact)
{
	struct kernel_sigaction
		tmp_act, tmp_oact,
		*rt_act  =  act ? &tmp_act  : NULL,
		*rt_oact = oact ? &tmp_oact : NULL;

	long ret;

	/* TODO test readability of memory */
	if (rt_act)
	{
		*rt_act = (struct kernel_sigaction)
		{
			.handler  = act->handler,
			.flags    = act->flags,
			.restorer = act->restorer,
			.mask     = act->mask,
		};
	}

	ret = user_rt_sigaction(sig, rt_act, rt_oact, sizeof(sigset_t));
	
	/* TODO test readability of memory */
	if (oact)
	{
		*oact = (struct kernel_old_sigaction)
		{
			.handler  = rt_oact->handler,
			.flags    = rt_oact->flags,
			.restorer = rt_oact->restorer,
			.mask     = rt_oact->mask,
		};
	}

	return ret;
}

long user_rt_sigaction(int sig, const struct kernel_sigaction *act,
                                      struct kernel_sigaction *oact, size_t sigsetsize)
{
	long ret = -EINVAL;
	struct kernel_sigaction wrap;
	thread_ctx_t *local_ctx = get_thread_ctx();

	if (sigsetsize != sizeof(kernel_sigset_t))
		return ret;

	/* TODO: do -EFAULT magic on SIGSEGV */
	if (act)
	{
		wrap = *act;

		if ( act->handler != (kernel_sighandler_t)SIG_ERR &&
		     act->handler != (kernel_sighandler_t)SIG_DFL &&
		     act->handler != (kernel_sighandler_t)SIG_IGN )
			wrap.handler = sigwrap_handler;

		wrap.flags |= SA_ONSTACK;
		wrap.flags &=~ ( SA_NODEFER | SA_RESTORER );
		memset(&wrap.mask, 0xff, sizeof(wrap.mask));
	}

	siglock(local_ctx);
	ret = sys_rt_sigaction(sig, act ? &wrap : NULL, NULL, sigsetsize);

	if (!ret && oact)
		*oact = local_ctx->sighandler->sigaction_list[sig];

	if (!ret && act)
	{
		local_ctx->sighandler->sigaction_list[sig] = *act;
		local_ctx->sighandler->sigaction_list[sig].mask.bitmask[0] &=~ ( 1<<(SIGKILL-1) | 1<<(SIGSTOP-1) );
	}
	sigunlock(local_ctx);

	return ret;
}

