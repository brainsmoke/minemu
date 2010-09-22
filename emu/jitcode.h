#ifndef JITCODE_H
#define JITCODE_H

#include <unistd.h>
#include <stdarg.h>

#include "lib.h"
#include "opcodes.h"

typedef struct
{
	char *jmp_addr;
	unsigned char imm, len;

} trans_t;

long imm_at(char *addr, long size);
void imm_to(char *dest, long imm);

int gen_code(char *dst, char *fmt, ...);

void translate_op(char *dest, instr_t *instr, trans_t *trans,
                  char *map, unsigned long map_len);

int generate_jump(char *jit_addr, char *dest, trans_t *trans, char *map, unsigned long map_len);
int generate_stub(char *jit_addr, char *jmp_addr, char *imm_addr);

#endif /* JITCODE_H */
