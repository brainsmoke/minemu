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
#define BAD_OP       (0x418)
#define CUTOFF_OP    (0x419)

typedef struct
{
	char *addr;
	unsigned short op;
	unsigned char mrm, imm, len, p[5];
} instr_t;

int read_op(char *addr, instr_t *instr, int max_len);
int op_size(char *addr, int max_len);

#endif /* OPCODES_H */
