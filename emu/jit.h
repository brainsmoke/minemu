#ifndef JIT_H
#define JIT_H

#include "opcodes.h"

void jit_init();
char *jit(char *addr);

#endif /* JIT_H */
