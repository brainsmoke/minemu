#ifndef JIT_FRAGMENT_H
#define JIT_FRAGMENT_H

#include <signal.h>

#include "opcodes.h"

/* non-re-entrant */
char *jit_fragment(char *code, long len, char *entry);
char *jit_fragment_run(struct sigcontext *context);

void get_xmm5(char *xmm5);
void get_xmm6(char *xmm6);
void get_xmm7(char *xmm7);

#endif /* JIT_FRAGMENT_H */
