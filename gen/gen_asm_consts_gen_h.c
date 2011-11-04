
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
#include "threads.h"

/* export some of our nice preprocessor defines to our linker file */

int main(void)
{
	printf("#define CTX__JIT_RETURN_ADDR (0x%lx)\n", offsetof(thread_ctx_t, jit_return_addr));
	printf("#define CTX__RUNTIME_IJMP_ADDR (0x%lx)\n", offsetof(thread_ctx_t, runtime_ijmp_addr));
	printf("#define CTX__SCRATCH_STACK_TOP (0x%lx)\n", offsetof(thread_ctx_t, scratch_stack_top));
	printf("#define CTX__USER_ESP (0x%lx)\n", offsetof(thread_ctx_t, user_esp));
	printf("#define CTX__USER_EIP (0x%lx)\n", offsetof(thread_ctx_t, user_eip));
	printf("#define CTX__JIT_EIP (0x%lx)\n", offsetof(thread_ctx_t, jit_eip));
	printf("#define CTX__JIT_FRAGMENT_RESTARTSYS (0x%lx)\n", offsetof(thread_ctx_t, jit_fragment_restartsys));
	printf("#define CTX__JIT_FRAGMENT_RUNNING (0x%lx)\n", offsetof(thread_ctx_t, jit_fragment_running));
	printf("#define CTX__JIT_FRAGMENT_ENTRY (0x%lx)\n", offsetof(thread_ctx_t, jit_fragment_entry));
	printf("#define CTX__JIT_FRAGMENT_SAVED_ESP (0x%lx)\n", offsetof(thread_ctx_t, jit_fragment_saved_esp));
	printf("#define CTX__IJMP_TAINT (0x%lx)\n", offsetof(thread_ctx_t, ijmp_taint));
	printf("#define CTX__FLAGS_TMP (0x%lx)\n", offsetof(thread_ctx_t, flags_tmp));
	printf("#define CTX__MY_ADDR (0x%lx)\n", offsetof(thread_ctx_t, my_addr));
	printf("#define CTX__SIZE (0x%lx)\n", sizeof(thread_ctx_t));
	assert( (sizeof(thread_ctx_t) & 0xfff) == 0);
	assert( (offsetof(thread_ctx_t, fault_page0) & 0xfff) == 0);
	assert( (offsetof(thread_ctx_t, sigwrap_stack) & 0xfff) == 0);
	assert( (offsetof(thread_ctx_t, jit_fragment_page) & 0xfff) == 0);
	assert( (offsetof(thread_ctx_t, scratch_stack) & 0xfff) == 0);

	exit(EXIT_SUCCESS);
}
