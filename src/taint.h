
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 * Copyright 2011 Vrije Universiteit Amsterdam
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

#ifndef TAINT_H
#define TAINT_H

#define TAINT_RET_TRAP_MASK (0x80808080)
#define TAINT_RET_TRAP_INVMASK (0x7f7f7f7f)

#ifndef __ASSEMBLER__

enum
{
	TAINT_ON,
	TAINT_OFF,
};

enum
{
	TAINT_CLEAR = 0x00,
	TAINT_SOCKADDR = 0x01,
	TAINT_POINTER = 0x02,
	TAINT_MALLOC_META = 0x04,
	TAINT_FREED = 0x08,
	TAINT_SOCKET = 0x10,
	TAINT_ENV = 0x20,
	TAINT_FILE = 0x40,
	TAINT_RET_TRAP = 0x80,
};

#define TAINT_RET_MALLOC (0x00000080UL)
#define TAINT_RET_2      (0x00008000UL)
#define TAINT_RET_3      (0x00008080UL)
#define TAINT_RET_4      (0x00800000UL)
#define TAINT_RET_5      (0x00800080UL)
#define TAINT_RET_6      (0x00808000UL)
#define TAINT_RET_7      (0x00808080UL)
#define TAINT_RET_8      (0x80000000UL)
#define TAINT_RET_9      (0x80000080UL)
#define TAINT_RET_10     (0x80008000UL)
#define TAINT_RET_11     (0x80008080UL)
#define TAINT_RET_12     (0x80800000UL)
#define TAINT_RET_13     (0x80800080UL)
#define TAINT_RET_14     (0x80808000UL)
#define TAINT_RET_15     (0x80808080UL)

#define TAINT_LONG(a) (0x01010101*(a))

extern int taint_flag;

extern char *trusted_dirs_default;
extern char *trusted_dirs;
int set_trusted_dirs(char *dirs);

void taint_mem(void *mem, unsigned long size, int type);
void taint_or(void *mem, unsigned long size, int type);
void taint_and(void *mem, unsigned long size, int type);
void do_taint(long ret, long call, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

void get_xmm5(unsigned char *xmm5);
void get_xmm6(unsigned char *xmm6);
void get_xmm7(unsigned char *xmm7);

void put_xmm5(unsigned char *xmm5);
void put_xmm6(unsigned char *xmm6);
void put_xmm7(unsigned char *xmm7);

unsigned long get_reg_taint(int reg);
void set_reg_taint(int reg, unsigned long val);

#endif

#endif /* TAINT_H */
