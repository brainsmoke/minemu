#ifndef JIT_ADHOC_H
#define JIT_ADHOC_H

#include "opcodes.h"

/* non-re-entrant */
char *jit_fragment_translate(char *code, long len, char *entry);

#endif /* JIT_ADHOC_H */
