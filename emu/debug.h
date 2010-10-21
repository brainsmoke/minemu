#ifndef DEBUG_H
#define DEBUG_H

#include "sigwrap.h"

void printhex(const void *data, int len);
void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane);

void print_sigcontext(struct sigcontext *sc);
void print_fpstate(struct _fpstate *fpstate);
void print_siginfo(siginfo_t *info);
void print_ucontext(struct kernel_ucontext *uc);
void print_rt_sigframe(struct kernel_rt_sigframe *frame);
void print_sigframe(struct kernel_sigframe *frame);

void print_sigcontext_diff(struct sigcontext *sc1, struct sigcontext *sc2);
void print_fpstate_diff(struct _fpstate *fpstate1, struct _fpstate *fpstate2);
void print_siginfo_diff(siginfo_t *info1, siginfo_t *info2);
void print_ucontext_diff(struct kernel_ucontext *uc1, struct kernel_ucontext *uc2);
void print_rt_sigframe_diff(struct kernel_rt_sigframe *frame1, struct kernel_rt_sigframe *frame2);
void print_sigframe_diff(struct kernel_sigframe *frame1, struct kernel_sigframe *frame2);

#ifdef EMU_DEBUG
void print_debug_data(void);
#endif

#endif /* DEBUG_H */
