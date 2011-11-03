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

#include "threads.h"
#include "syscalls.h"
#include "runtime.h"
#include "error.h"
#include "mm.h"

static thread_ctx_t __attribute__ ((aligned (0x1000))) ctx[MAX_THREADS];
static sighandler_ctx_t sighandler;
static file_ctx_t files;
static long thread_lock=0;

void set_thread_ctx(thread_ctx_t *local_ctx)
{
	long ret = sys_mmap2(local_ctx, sizeof(thread_ctx_t),
	                     PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (ret != (long)local_ctx)
		die("set_thread_ctx(): mmap() failed\n");

	local_ctx->my_addr = local_ctx;

	local_ctx->jit_return_addr = jit_return;
	local_ctx->runtime_ijmp_addr = runtime_ijmp;
	local_ctx->jit_fragment_exit_addr = jit_fragment_exit;

	local_ctx->sigwrap_stack_top = &local_ctx->sigwrap_stack[sizeof(local_ctx->sigwrap_stack)/sizeof(long)-1];
	local_ctx->scratch_stack_top = &local_ctx->user_esp;

	local_ctx->files = &files;
	local_ctx->sighandler = &sighandler;

	init_tls(local_ctx, sizeof(thread_ctx_t));
	sys_mprotect(&local_ctx->fault_page0, 0x1000, PROT_NONE);
	protect_ctx();
}

void protect_ctx(void)
{
	sys_mprotect(get_thread_ctx()->jit_fragment_page, PG_SIZE, PROT_EXEC|PROT_READ);
}

void unprotect_ctx(void)
{
	sys_mprotect(get_thread_ctx()->jit_fragment_page, PG_SIZE, PROT_READ|PROT_WRITE);
}

void init_threads(void)
{
	set_thread_ctx(&ctx[0]);
}

long user_clone(unsigned long flags, unsigned long sp, void *parent_tid, long dummy, void *child_tid)
{
	long ret;
	// allocate context / relocate stack
	ret = sys_clone(flags, sp, parent_tid, dummy, child_tid);
	// if error: free context
	return ret;
}

void user_exit(long status)
{
	// free context
	mutex_unlock_exit(status, &thread_lock);
}

