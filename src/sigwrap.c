
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


#include <stdlib.h>
#include <string.h>

#include "syscalls.h"
#include "sigwrap.h"
#include "scratch.h"
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
#include "exec_ctx.h"

/*
 * wrapper around signals, preventing signals being delivered on the
 * runtime stack, or otherwise within an emulated opcode which would be
 * atomic when executed natively
 */

extern kernel_sigset_t old_sigset;

/* for the emulation of some some non-blocking syscalls, we block all signals
 *
 */
int try_block_signals(void)
{
	kernel_sigset_t blockall;
	memset(&blockall, 0, sizeof(blockall));
	return !syscall_intr(__NR_sigprocmask, SIG_BLOCK, (long)&blockall, (long)&old_sigset, 0,0,0);
}

void unblock_signals(void)
{
	syscall_intr(__NR_sigprocmask, SIG_SETMASK, (long)&old_sigset, (long)NULL, 0,0,0);
}

/* our sighandler bookkeeping:
 *
 */
static stack_t user_altstack;
static struct kernel_sigaction user_sigaction_list[KERNEL_NSIG];

/* INTERCAL's COME FROM is for wimps :-)
 *
 */
static void finish_instruction(struct sigcontext *context)
{
	char *orig_eip, *jit_op_start;
	long jit_op_len;

	if ( (orig_eip = jit_rev_lookup_addr((char *)context->eip, &jit_op_start, &jit_op_len)) )
	{
		if ( (char *)context->eip == jit_op_start ) /* we don't have to do anything */
		{
			context->eip = (long)orig_eip; /* set return instruction pointer to user eip */
			return;
		}
		/* jit the jit! */
		context->eip = (long)jit_fragment(jit_op_start, jit_op_len, (char *)context->eip);
	}
	else if ( between(syscall_intr_critical_start, syscall_intr_critical_end, (char *)context->eip) )
	{
		context->eip = (long)syscall_intr_critical_start;
	}
	else if ( between(runtime_cache_resolution_start, runtime_cache_resolution_end, (char *)context->eip) )
	{
		context->eip += reloc_runtime_cache_resolution_start-runtime_cache_resolution_start;
	}

	runtime_ijmp_addr = reloc_runtime_ijmp;
	jit_return_addr = reloc_jit_return;
	jit_fragment_run(context);
	runtime_ijmp_addr = runtime_ijmp;
	jit_return_addr = jit_return;

	orig_eip = jit_rev_lookup_addr((char *)context->eip, &jit_op_start, &jit_op_len);
	if ( (char *)context->eip != jit_op_start )
		die("instruction pointer (%x) not at opcode start after jit_fragment_run()", context->eip);

	context->eip = (long)orig_eip;

	if (get_exec_ctx()->jit_fragment_restartsys)
		context->eip -= 2;
}

#define __USER_CS (0x73)
#define __USER_DS (0x7b)

#ifndef SA_RESTORER
#define SA_RESTORER        (0x04000000)
#endif

void *get_sigframe_addr(struct kernel_sigaction *action, struct sigcontext *context, long size)
{
    unsigned long sp = context->esp;

	if ( contains(user_altstack.ss_sp, user_altstack.ss_size, (char*)sp) &&
	    !contains(user_altstack.ss_sp, user_altstack.ss_size, (char*)(sp-size)) )
		/* XXX this will segfault/terminate the program, just as the normal program would,
		 * mirrors kernel code and it's simple, but it's dirty^2 doing it this way
		 */
		return (void *) -1L;

	if ( (action->flags & SA_ONSTACK) &&
	     (user_altstack.ss_size > 0) &&
	     (!contains(user_altstack.ss_sp, user_altstack.ss_size, (char *)sp)) )
		sp = (unsigned long) user_altstack.ss_sp - user_altstack.ss_size;

	else if ( ((context->ss & 0xffff) != __USER_DS) &&
	         !(action->flags & SA_RESTORER) &&
	          (action->restorer) )
		sp = (unsigned long) action->restorer;

	sp -= size;
	sp = ((sp + 4) & -16ul) - 4; /* alignment required by ABI */
	return (void*)sp;
}


static struct kernel_sigframe *copy_sigframe(struct kernel_sigframe *frame,
                                             struct kernel_sigaction *action,
                                             struct sigcontext *context)
{
	struct kernel_sigframe *copy =
		(struct kernel_sigframe *)get_sigframe_addr(action, context, sizeof(*frame));

	*copy = *frame;

	if (frame->pretcode == &frame->retcode[0])
		copy->pretcode = &copy->retcode[0];

	return copy;
}

static struct kernel_rt_sigframe *copy_rt_sigframe(struct kernel_rt_sigframe *frame,
                                                   struct kernel_sigaction *action,
                                                   struct sigcontext *context)
{
	struct kernel_rt_sigframe *copy =
		(struct kernel_rt_sigframe *)get_sigframe_addr(action, context, sizeof(*frame));

	*copy = *frame;

	if (frame->pretcode == &frame->retcode[0])
		copy->pretcode = &copy->retcode[0];

	if (frame->pinfo == &frame->info)
		copy->pinfo = &copy->info;

	if (frame->puc == &frame->uc)
		copy->puc = &copy->uc;

	copy->uc.uc_stack = user_altstack;

	return copy;
}


static void sigwrap_handler(int sig, siginfo_t *info, void *_)
{
	struct kernel_rt_sigframe *rt_sigframe = (struct kernel_rt_sigframe *) (((long)&sig)-4);
	struct kernel_sigframe    *sigframe    = (struct kernel_sigframe *)    (((long)&sig)-4);
	struct kernel_sigaction *action = &user_sigaction_list[sig];
	struct _fpstate *fpstate;
	struct sigcontext *context;
	unsigned long *sigmask, *extramask;

	if ( (sig < 0) || (sig >= KERNEL_NSIG) )
		die("bad signo. %d", sig);

	if ( action->flags & SA_SIGINFO )
	{
		context = &rt_sigframe->uc.uc_mcontext;
		sigmask = &rt_sigframe->uc.uc_sigmask.bitmask[0];
		extramask = &rt_sigframe->uc.uc_sigmask.bitmask[1];
		fpstate = &rt_sigframe->fpstate;
	}
	else
	{
		context = &sigframe->sc;
		sigmask = &sigframe->sc.oldmask;
		extramask = &sigframe->extramask[0];
		fpstate = &sigframe->fpstate;
	}

	if ( (sig == SIGSEGV) || (sig == SIGILL) )
	{
		user_eip = (long)jit_rev_lookup_addr((char *)context->eip, NULL, NULL);
		long regs[] = { context->eax, context->ecx, context->edx, context->ebx,
		                context->esp, context->ebp, context->esi, context->edi, };
		do_taint_dump(regs);
	}

	finish_instruction(context);

	get_xmm5((unsigned char *)&fpstate->_xmm[5]);
	get_xmm6((unsigned char *)&fpstate->_xmm[6]);
	get_xmm7((unsigned char *)&fpstate->_xmm[7]);

	if ( action->flags & SA_ONESHOT )
	{
		unshield();
		memset(action, 0, sizeof(*action));
		shield();
	}

	if ( action->flags & SA_SIGINFO )
	{
		rt_sigframe = copy_rt_sigframe(rt_sigframe, action, context);
		taint_mem(rt_sigframe, sizeof(*rt_sigframe), 0x00);
	}
	else
	{
		sigframe = copy_sigframe(sigframe, action, context);
		taint_mem(sigframe, sizeof(*sigframe), 0x00);
	}

	/* Most evil hack ever! We 'deliver' the user's signal by modifying our own sigframe
	 * to match the user process' state at signal delivery, and call sigreturn.
	 */

	context->ds =
	context->es =
	context->ss = __USER_DS;
	context->cs = __USER_CS;

	user_eip = (long)action->handler;   /* jump into jit (or in this case runtime) */
	context->eip = (long)state_restore; /* code, not user code                     */
	context->eax = sig;

	if ( action->flags & SA_SIGINFO )
	{
		context->esp = (long)rt_sigframe;
		context->ecx = (long)&rt_sigframe->uc;
		context->edx = (long)&rt_sigframe->info;
//print_rt_sigframe(rt_sigframe);
	}
	else
	{
//print_sigframe(sigframe);
		context->esp = (long)sigframe;
		context->ecx = 0;
		context->edx = 0;
	}

	*sigmask   |= action->mask.bitmask[0];
	*extramask |= action->mask.bitmask[1];

	/* let the kernel 'deliver' a signal using (rt_)sigreturn! */
}

void user_sigreturn(void)
{
	long *esp = (long *)scratch_stack[0]; /* ugly */
	struct kernel_sigframe *sigframe = (struct kernel_sigframe *)&esp[-2];
	struct kernel_sigframe frame = *sigframe;
	struct sigcontext *context = &frame.sc;
	
	user_eip = context->eip;            /* jump into jit code, */
	context->eip = (long)state_restore; /* not user code       */
	load_sigframe(__NR_sigreturn, &frame);
}

void user_rt_sigreturn(void)
{
	long *esp = (long *)scratch_stack[0]; /* ugly */
	struct kernel_rt_sigframe *rt_sigframe = (struct kernel_rt_sigframe *)&esp[-1];
	struct kernel_rt_sigframe frame = *rt_sigframe;
	struct sigcontext *context = &frame.uc.uc_mcontext;
	user_eip = context->eip;            /* jump into jit code, */
	context->eip = (long)state_restore; /* not user code       */
	frame.uc.uc_stack = (stack_t)
	{
		.ss_sp = &get_exec_ctx()->sigwrap_stack,
		.ss_flags = 0,
		.ss_size = sizeof( ctx[0].sigwrap_stack )
	};
	load_rt_sigframe(__NR_rt_sigreturn, &frame);
}

static void altstack_setup(void)
{
	long ret;

	stack_t sigwrap_altstack =
	{
		.ss_sp = &get_exec_ctx()->sigwrap_stack,
		.ss_flags = 0,
		.ss_size = sizeof( ctx[0].sigwrap_stack )
	};

	if ( (ret=sys_sigaltstack(&sigwrap_altstack, &user_altstack)) )
		die("altstack_setup: sigaltstack failed: %d", ret);
}

static void altstack_restore(void)
{
	long ret;
	if ( (ret=sys_sigaltstack(&user_altstack, NULL)) )
		die("altstack_restore: sigaltstack failed: %d", ret);
}

/* should be called at the start
 *
 */
void sigwrap_init(void)
{
	altstack_setup();
	memset(user_sigaction_list, 0, KERNEL_NSIG*sizeof(struct kernel_sigaction));

	struct kernel_sigaction act =
	{
		.handler = sigwrap_handler,
		.flags = SA_ONSTACK,
	};

	memset(&act.mask, 0xff, sizeof(act.mask));
	user_rt_sigaction(SIGSEGV, &act, NULL, sizeof(act.mask));
	user_rt_sigaction(SIGILL, &act, NULL, sizeof(act.mask));
}

/* the emulator blocks signals */
/* TODO pointer check, ON_STACK flag */
long user_sigaltstack(const stack_t *ss, stack_t *oss)
{
	long ret;
	altstack_restore();
	ret = sys_sigaltstack(ss, oss);
	altstack_setup();
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
	long ret;
	struct kernel_sigaction wrap;

	/* TODO test readability of memory */
	if (act)
	{
		wrap = *act;
		wrap.handler = sigwrap_handler;
		wrap.flags |= SA_ONSTACK;
		wrap.flags &=~ SA_NODEFER;
		memset(&wrap.mask, 0xff, sizeof(wrap.mask));
	}

	ret = sys_rt_sigaction(sig, act, oact, sigsetsize);

	if (ret)
		return ret;

	if (oact)
		*oact = user_sigaction_list[sig];

	if (act)
	{
		if ( act->handler == (kernel_sighandler_t)SIG_ERR ||
		     act->handler == (kernel_sighandler_t)SIG_DFL ||
		     act->handler == (kernel_sighandler_t)SIG_IGN )
			user_sigaction_list[sig] = *act;
		else
			ret = sys_rt_sigaction(sig, &wrap, &user_sigaction_list[sig], sigsetsize);
	}

	if (ret)
		die("user_rt_sigaction: sys_rt_sigaction() failed %d", ret);

	return ret;
}

