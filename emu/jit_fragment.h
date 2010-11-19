#ifndef JIT_FRAGMENT_H
#define JIT_FRAGMENT_H

#include <signal.h>

#include "opcodes.h"

/* non-re-entrant */
char *jit_fragment(char *code, long len, char *entry);
char *jit_fragment_run(struct sigcontext *context);

#endif /* JIT_FRAGMENT_H */
