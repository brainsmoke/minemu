
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
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

#ifndef JIT_CODE_H
#define JIT_CODE_H

#include <unistd.h>
#include <stdarg.h>

#include "lib.h"
#include "opcodes.h"
#include "hooks.h"

enum
{
	LAZY_CALL,
	PREFETCH_ON_CALL,
	PRESEED_ON_CALL,
};

extern int call_strategy;

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

int generate_hook(char *dest, hook_func_t func);

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

#define CMPXCHG8               (0x42)
#define CMPXCHG                (0x43)
#define CMPXCHG8B              (0x44)
#define CPUID                  (0x45)

#define JOIN (CONTROL|12)

extern const unsigned char jit_action[];

#endif /* JIT_CODE_H */
