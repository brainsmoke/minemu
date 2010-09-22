#ifndef SIGWRAP_H
#define SIGWRAP_H

#include <signal.h>

void user_sigaltstack(const stack_t *ss, stack_t *oss);
int user_sigaction(int sig, const struct old_sigaction *act, struct old_sigaction *oact);
long user_rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact, size_t sigsetsize);
long user_rt_sigreturn(void);
unsigned long user_signal(int sig, __sighandler_t handler);

#endif /* SIGWRAP_H */
