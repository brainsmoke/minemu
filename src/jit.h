#ifndef JIT_H
#define JIT_H

#include "opcodes.h"

void jit_init();
char *jit(char *addr);
char *jit_lookup_addr(char *addr);
char *jit_rev_lookup_addr(char *jit_addr, char **jit_op_start, long *jit_op_len);

#endif /* JIT_H */
