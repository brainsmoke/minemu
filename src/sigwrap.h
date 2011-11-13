
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

#ifndef SIGWRAP_H
#define SIGWRAP_H

#include <signal.h>
#include <ucontext.h>


#define KERNEL_NSIG (64)
typedef struct
{
	unsigned long bitmask[KERNEL_NSIG/8/sizeof(long)];

} kernel_sigset_t;

struct kernel_ucontext
{
    unsigned long uc_flags;
    struct kernel_ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    kernel_sigset_t uc_sigmask;
};

typedef void (*kernel_sighandler_t)(int, siginfo_t *, void *);

struct kernel_old_sigaction
{
	kernel_sighandler_t handler;
	kernel_sigset_t mask;
	unsigned long flags;
	void (*restorer) (void);
};

struct kernel_sigaction {
	kernel_sighandler_t handler;
	unsigned long flags;
	void (*restorer) (void);
	kernel_sigset_t mask;
};

struct kernel_sigframe {
    char *pretcode;
    int sig;
    struct sigcontext sc;
    struct _fpstate fpstate_unused;
    unsigned long extramask[KERNEL_NSIG/8/sizeof(long)-1];
    char retcode[8];
	/* fpstate */
};

struct kernel_rt_sigframe {
    char *pretcode;
    int sig;
    struct siginfo *pinfo;
    void *puc;
    struct siginfo info;
    struct kernel_ucontext uc;
    char retcode[8];
	/* fpstate */
};

int try_block_signals(void);
int block_signals(void);
void unblock_signals(void);
void altstack_setup(void);
void sigwrap_init(void);
void load_sigframe(struct kernel_sigframe *frame);
void load_rt_sigframe(struct kernel_rt_sigframe *frame);

long user_sigaltstack(const stack_t *ss, stack_t *oss);

long user_sigaction(int sig, const struct kernel_old_sigaction *act,
                                   struct kernel_old_sigaction *oact);

long user_rt_sigaction(int sig, const struct kernel_sigaction *act,
                                      struct kernel_sigaction *oact, size_t sigsetsize);

void user_sigreturn(void);
void user_rt_sigreturn(void);

void do_sigreturn(void);
void do_rt_sigreturn(void);

unsigned long user_signal(int sig, void (*handler) (int, siginfo_t *, void *));

/* used for restoring pre-signal-state */
void get_xmm5(unsigned char *xmm5);
void get_xmm6(unsigned char *xmm6);
void get_xmm7(unsigned char *xmm7);

#endif /* SIGWRAP_H */
