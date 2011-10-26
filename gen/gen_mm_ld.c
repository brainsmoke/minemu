
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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <assert.h>

#include "mm.h"
#include "exec_ctx.h"

/* export some of our nice preprocessor defines to our linker file */

int main(void)
{
	printf(".user_end = 0x%lx;\n", USER_END);
	printf(".jit_end = 0x%lx;\n", JIT_END);
	printf("minemu_stack_bottom = 0x%lx;\n", MINEMU_STACK_BOTTOM);

	printf("ctx__jmp_cache = 0x%lx;\n", offsetof(exec_ctx_t, jmp_cache));
	printf("ctx__fault_page0 = 0x%lx;\n", offsetof(exec_ctx_t, fault_page0));
	printf("ctx__sigwrap_stack = 0x%lx;\n", offsetof(exec_ctx_t, sigwrap_stack));
	printf("ctx__jit_fragment_page = 0x%lx;\n", offsetof(exec_ctx_t, jit_fragment_page));
	printf("ctx__jit_return_addr = 0x%lx;\n", offsetof(exec_ctx_t, jit_return_addr));
	printf("ctx__runtime_ijmp_addr = 0x%lx;\n", offsetof(exec_ctx_t, runtime_ijmp_addr));
	printf("ctx__sigwrap_stack_top = 0x%lx;\n", offsetof(exec_ctx_t, sigwrap_stack_top));
	printf("ctx__scratch_stack_top = 0x%lx;\n", offsetof(exec_ctx_t, scratch_stack_top));
	printf("ctx__my_addr = 0x%lx;\n", offsetof(exec_ctx_t, my_addr));
	printf("ctx__scratch_stack = 0x%lx;\n", offsetof(exec_ctx_t, scratch_stack));
	printf("ctx__user_eip = 0x%lx;\n", offsetof(exec_ctx_t, user_eip));
	printf("ctx__jit_eip = 0x%lx;\n", offsetof(exec_ctx_t, jit_eip));
	printf("ctx__jit_fragment_restartsys = 0x%lx;\n", offsetof(exec_ctx_t, jit_fragment_restartsys));
	printf("ctx__jit_fragment_running = 0x%lx;\n", offsetof(exec_ctx_t, jit_fragment_running));
	printf("ctx__jit_fragment_scratch = 0x%lx;\n", offsetof(exec_ctx_t, jit_fragment_scratch));
	printf("ctx__ijmp_taint = 0x%lx;\n", offsetof(exec_ctx_t, ijmp_taint));
	printf("ctx__taint_tmp = 0x%lx;\n", offsetof(exec_ctx_t, taint_tmp));
	printf("ctx__size = 0x%lx;\n", sizeof(exec_ctx_t));
	printf("ctx_array__size = 0x%lx;\n", sizeof(exec_ctx_t)*MAX_THREADS);
	assert( (sizeof(exec_ctx_t) & 0xfff) == 0);

	exit(EXIT_SUCCESS);
}
