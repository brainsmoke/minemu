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

#ifndef EXEC_CTX_H
#define EXEC_CTX_H

#include <stddef.h>
#include "tls_segment.h"

#define JMP_CACHE_SIZE (0x10000)
#define MAX_THREADS 32

typedef struct
{
	char *addr;
	char *jit_addr;

} jmp_map_t;

typedef struct exec_ctx_s exec_ctx_t;

typedef long (*ijmp_t)(void);

struct exec_ctx_s
{
	jmp_map_t jmp_cache[JMP_CACHE_SIZE];

	char fault_page0[0x1000];

	long sigwrap_stack[0x800];

	char jit_fragment_page[0x1000 - 2*sizeof(ijmp_t) -
	                                2*sizeof(long *) -
	                                  sizeof(exec_ctx_t *)];
	ijmp_t jit_return_addr;
	ijmp_t runtime_ijmp_addr;
	long *sigwrap_stack_top;
	long *scratch_stack_top;
	exec_ctx_t *my_addr;

	long scratch_stack[0x2000 - 7];

	long user_eip;
	long jit_eip;

	long jit_fragment_restartsys;
	long jit_fragment_running;
	long jit_fragment_scratch;

	long ijmp_taint;
	long taint_tmp;

};

inline exec_ctx_t *get_exec_ctx(void)
{
	return (exec_ctx_t *)get_tls_long(offsetof(exec_ctx_t, my_addr));
}

extern exec_ctx_t ctx[MAX_THREADS];

void init_exec_ctx(void);

#endif /* EXEC_CTX_H */
