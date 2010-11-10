#ifndef JIT_CODE_H
#define JIT_CODE_H

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

int jump_to(char *dest, char *jmp_addr);

int gen_code(char *dst, char *fmt, ...);

int generate_ill(char *dest, trans_t *trans);

void translate_op(char *dest, instr_t *instr, trans_t *trans,
                  char *map, unsigned long map_len);

int generate_jump(char *jit_addr, char *dest, trans_t *trans, char *map, unsigned long map_len);
int generate_stub(char *jit_addr, char *jmp_addr, char *imm_addr);

#define COPY_INSTRUCTION       (0)

#define UNDEFINED_INSTRUCTION  (1)

#define CONDITIONAL_MOVE       (2)

#define CONTROL                (0x20)
#define CONTROL_MASK         (~(CONTROL-1))

#define JUMP_RELATIVE          (CONTROL|0)
#define JUMP_CONDITIONAL       (CONTROL|1)
#define JUMP_FAR               (CONTROL|2)
#define JUMP_INDIRECT          (CONTROL|3)

#define LOOP                   (CONTROL|4)

#define CALL_RELATIVE          (CONTROL|5)
#define CALL_FAR             /*(CONTROL|6)*/ U
#define CALL_INDIRECT          (CONTROL|7)

#define RETURN                 (CONTROL|8)
#define RETURN_CLEANUP         (CONTROL|9)
#define RETURN_FAR           /*(CONTROL|10)*/ U

#define INT                    (0x40)
#define SYSENTER               (0x41)

#define JOIN (CONTROL|12)

extern const unsigned char jit_action[];

#endif /* JIT_CODE_H */
