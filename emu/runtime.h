#ifndef RUNTIME_H
#define RUNTIME_H

#include "scratch.h"
#include "codemap.h"

void enter(long eip, long esp);

long runtime_ijmp();
extern long (*runtime_ijmp_addr)(void);

long int80_emu();
extern long (*int80_emu_addr)(void);
long linux_sysenter_emu();
extern long (*linux_sysenter_emu_addr)(void);

extern char syscall_hooks[N_SYSCALL_HOOKS];

void pre_ptrace_syscall();
void post_ptrace_syscall();

#endif /* RUNTIME_H */
