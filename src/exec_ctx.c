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

#include <sys/mman.h>

#include "exec_ctx.h"
#include "syscalls.h"
#include "runtime.h"
#include "error.h"
#include "mm.h"

extern char fd_type[1024];
extern struct kernel_sigaction user_sigaction_list[KERNEL_NSIG];

void set_exec_ctx(exec_ctx_t *local_ctx)
{
	long ret = sys_mmap2(local_ctx, sizeof(exec_ctx_t),
	                     PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (ret != (long)local_ctx)
		die("set_exec_ctx(): mmap() failed\n");

	local_ctx->my_addr = local_ctx;

	local_ctx->jit_return_addr = jit_return;
	local_ctx->runtime_ijmp_addr = runtime_ijmp;
	local_ctx->jit_fragment_exit_addr = jit_fragment_exit;

	local_ctx->sigwrap_stack_top = &local_ctx->sigwrap_stack[sizeof(local_ctx->sigwrap_stack)/sizeof(long)-1];
	local_ctx->scratch_stack_top = &local_ctx->user_esp;

	local_ctx->fd_type = fd_type;
	local_ctx->sigaction_list = user_sigaction_list;

	init_tls(local_ctx, sizeof(exec_ctx_t));
	sys_mprotect(&local_ctx->fault_page0, 0x1000, PROT_NONE);
	protect_ctx();
}

void protect_ctx(void)
{
	sys_mprotect(get_exec_ctx()->jit_fragment_page, PG_SIZE, PROT_EXEC|PROT_READ);
}

void unprotect_ctx(void)
{
	sys_mprotect(get_exec_ctx()->jit_fragment_page, PG_SIZE, PROT_READ|PROT_WRITE);
}

void init_exec_ctx(void)
{
	set_exec_ctx(&ctx[0]);
}
