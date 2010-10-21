#ifndef RUNTIME_H
#define RUNTIME_H

#include "scratch.h"

void emu_start(void *eip, long *esp);
void state_restore(void);

long runtime_ijmp(void);
long runtime_ret(void);
extern long (*runtime_ijmp_addr)(void);
extern long (*runtime_ret_addr)(void);

long jit_fragment_exit(void);
extern long (*jit_fragment_exit_addr)(void);

long int80_emu(void);
extern long (*int80_emu_addr)(void);
long linux_sysenter_emu(void);
extern long (*linux_sysenter_emu_addr)(void);

#endif /* RUNTIME_H */
