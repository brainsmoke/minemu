#ifndef OPCODES_H
#define OPCODES_H

#include <unistd.h>

#include "lib.h"

#define MAIN_OPTABLE (0x000)
#define ESC_OPTABLE  (0x100)
#define G38_OPTABLE  (0x200)
#define G3A_OPTABLE  (0x300)
#define GF6_OPTABLE  (0x400)
#define GF7_OPTABLE  (0x408)
#define GFF_OPTABLE  (0x410)

#define CODE_JOIN (-1)
#define CODE_STOP (-2)
#define CODE_ERR  (-3)

#define COPY_INSTRUCTION       (0)

#define UNDEFINED_INSTRUCTION  (1)

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
#define TAINT_AX_REG        (0)
#define TAINT_STR_TO_STR    (0)
#define TAINT_REG           (0)
#define TAINT_MODRM         (0)
#define TAINT_PUSH          (0)
#define TAINT_HIGH_REG      (0)
#define TAINT_DX            (0)
#define TAINT_AX            (0)

typedef struct
{
	char *addr;
	unsigned char mrm, imm, len, action, p1, p2, p3, p4;
} instr_t;

int read_op(char *addr, instr_t *instr, int max_len);
int op_size(char *addr, int max_len);

#endif /* OPCODES_H */
