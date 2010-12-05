
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
