#ifndef OPCODES_H
#define OPCODES_H

#include <unistd.h>

#include "lib.h"

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


typedef struct
{
	char *addr;
	unsigned char mrm, imm, len, type, action, err, p1, p2, p3, p4;
} instr_t;

int read_op(char *addr, instr_t *instr, int max_len);
int op_size(char *addr, int max_len);

#endif /* OPCODES_H */
