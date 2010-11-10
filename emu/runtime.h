#ifndef RUNTIME_H
#define RUNTIME_H

#include "scratch.h"

void emu_start(void *eip, long *esp);
void state_restore(void);

long runtime_ijmp(void);
long runtime_ret_cleanup(void);
long jit_fragment_exit(void);

long int80_emu(void);
long linux_sysenter_emu(void);

#endif /* RUNTIME_H */
