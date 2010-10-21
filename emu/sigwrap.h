#ifndef SIGWRAP_H
#define SIGWRAP_H

#include <signal.h>
#include <ucontext.h>


int try_block_signals(void);
void unblock_signals(void);
void sigwrap_init(void);
void load_sigframe(long call, void *frame);

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

struct kernel_old_sigaction
{
	void (*handler) (int, siginfo_t *, void *);
	kernel_sigset_t mask;
	unsigned long flags;
	void (*restorer) (void);
};

struct kernel_sigaction {
	void (*handler) (int, siginfo_t *, void *);
	unsigned long flags;
	void (*restorer) (void);
	kernel_sigset_t mask;
};

struct kernel_sigframe {
    char *pretcode;
    int sig;
    struct sigcontext sc;
    struct _fpstate fpstate;
    unsigned long extramask[KERNEL_NSIG/8/sizeof(long)-1];
    char retcode[8];
};

struct kernel_rt_sigframe {
    char *pretcode;
    int sig;
    struct siginfo *pinfo;
    void *puc;
    struct siginfo info;
    struct kernel_ucontext uc;
    struct _fpstate fpstate;
    char retcode[8];
};

long user_sigaltstack(const stack_t *ss, stack_t *oss);

long user_sigaction(int sig, const struct kernel_old_sigaction *act,
                                   struct kernel_old_sigaction *oact);

long user_rt_sigaction(int sig, const struct kernel_sigaction *act,
                                      struct kernel_sigaction *oact, size_t sigsetsize);

long user_sigreturn(void);
long user_rt_sigreturn(void);

unsigned long user_signal(int sig, void (*handler) (int, siginfo_t *, void *));

#endif /* SIGWRAP_H */
