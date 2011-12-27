
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

#ifndef HOOKS_H
#define HOOKS_H

#include "codemap.h"

extern char *hooklist;

int parse_hooklist(char *s);

typedef int (*hook_func_t)(long *);

typedef struct
{
	unsigned long long inode, dev, offset;
	unsigned long mtime;

	hook_func_t func;

} hook_t;

extern hook_t hook_table[];
extern int n_hooks;

int register_hook(hook_func_t func, unsigned long long inode,
                                    unsigned long long dev,
                                    unsigned long mtime,
                                    unsigned long long offset);

hook_func_t get_hook_func(code_map_t *map, unsigned long offset);


int fmt_check(long *regs);
int ping(long *regs);
int fault(long *regs);

#endif /* HOOKS_H */
