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

#define TAINT                  (0x80)
#define TAINT_MASK           (~(TAINT-1))

#define TAINT_BYTE         (0)

#define TAINT_OR           (0)
#define TAINT_XOR          (0)
#define TAINT_COPY         (0)
#define TAINT_COPY_ZX      (0)
#define TAINT_SWAP         (0)
#define TAINT_ERASE        (0)
#define TAINT_PUSHA        (0)
#define TAINT_POPA         (0)
#define TAINT_LEA          (0)
#define TAINT_LEAVE        (0)

#define TAINT_REG_TO_MODRM  (0)
#define TAINT_MODRM_TO_REG  (0)
#define TAINT_REG_TO_PUSH   (0)
#define TAINT_MODRM_TO_PUSH (0)
#define TAINT_POP_TO_REG    (0)
#define TAINT_POP_TO_MODRM  (0)
#define TAINT_AX_TO_OFFSET  (0)
#define TAINT_OFFSET_TO_AX  (0)
#define TAINT_AX_TO_REG     (0)
#define TAINT_AX_TO_STR     (0)
#define TAINT_STR_TO_AX     (0)
#define TAINT_AX_DX         (0)
#define TAINT_STR_TO_STR    (0)
#define TAINT_REG           (0)
#define TAINT_MODRM         (0)
#define TAINT_PUSH          (0)
#define TAINT_HIGH_REG      (0)
#define TAINT_DX            (0)
#define TAINT_AX            (0)

extern const unsigned char jit_action[];

#endif /* JIT_CODE_H */
