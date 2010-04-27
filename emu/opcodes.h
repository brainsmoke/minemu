#ifndef OPCODES_H
#define OPCODES_H

#include <unistd.h>

#include "lib.h"

#define CODE_JOIN (-1)
#define CODE_STOP (-2)
#define CODE_ERR  (-3)

typedef struct
{
	char *addr;
	unsigned char mrm, imm, len, type, action, err, p[4];
} instr_t;

typedef struct
{
	char *jmp_addr;
	unsigned char imm, len;

} trans_t;

int read_op(char *addr, instr_t *instr, int max_len);
int op_size(char *addr, int max_len);

long imm_at(char *addr, long size);
void imm_to(char *dest, long imm);

void translate_op(char *dest, instr_t *instr, trans_t *trans,
                  char *map, unsigned long map_len);

int generate_jump(char *jit_addr, char *dest, trans_t *trans, char *map, unsigned long map_len);
int generate_stub(char *jit_addr, char *jmp_addr, char *imm_addr);


#endif /* OPCODES_H */
