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
#include <sched.h>

#include "threads.h"
#include "syscalls.h"
#include "runtime.h"
#include "error.h"
#include "mm.h"
#include "jmp_cache.h"

static thread_ctx_t __attribute__ ((aligned (0x1000))) ctx[MAX_THREADS];
static sighandler_ctx_t sighandler;
static file_ctx_t files;
static long thread_lock;

char ctx_map[MAX_THREADS];

static thread_ctx_t *alloc_ctx(void)
{
	int i;
	for (i=0; i<MAX_THREADS; i++)
		if (ctx_map[i] == 0)
		{
			ctx_map[i] = 1;
			return &ctx[i];
		}

	return NULL;
}

static void free_ctx(thread_ctx_t *c)
{
	int me = ((unsigned long)c - (unsigned long)ctx)/sizeof(thread_ctx_t);
	ctx_map[me] = 0;
}

static void unshare_ctx(thread_ctx_t *c)
{
	int me = ((unsigned long)c - (unsigned long)ctx)/sizeof(thread_ctx_t);
	int i;
	for (i=0; i<MAX_THREADS; i++)
		if (i != me)
			ctx_map[i] = 0;
}

static void init_thread_ctx(thread_ctx_t *local_ctx)
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

	sys_mprotect(&local_ctx->fault_page0, 0x1000, PROT_NONE);
	sys_mprotect(&local_ctx->jit_fragment_page, PG_SIZE, PROT_EXEC|PROT_READ);
}

/* no need for locking, the only risk is doing too much work */
void purge_caches(char *addr, unsigned long len)
{
	int i;
	for (i=0; i<MAX_THREADS; i++)
		if (ctx_map[i] == 1)
			clear_jmp_cache(&ctx[i], addr, len);
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
	thread_ctx_t *new_ctx = alloc_ctx();
	mutex_init(&thread_lock);
	init_thread_ctx(new_ctx);
	init_tls(new_ctx, sizeof(thread_ctx_t));
}

/* release the lock and exit, without thouching the stack after
 * releasing the lock
 */
void mutex_unlock_exit(long status, long *lock);

long clone_relocate_stack(unsigned long flags, unsigned long sp,
                          void *parent_tid, void *tls, void *child_tid,
                          long stack_diff);

long user_clone(unsigned long flags, unsigned long sp, void *parent_tid, void *tls, void *child_tid)
{
	long ret;
	thread_ctx_t *child_ctx = NULL;

	if (flags & CLONE_VM)
	{
		mutex_lock(&thread_lock);
		child_ctx = alloc_ctx();
		mutex_unlock(&thread_lock);
		init_thread_ctx(child_ctx);
		int stack_diff = (long)child_ctx - (long)get_thread_ctx();
		/* I need to change a "this is the most ugly hack ever" comment somewhere else */
		ret = clone_relocate_stack(flags, sp, parent_tid, tls, child_tid, stack_diff);
		/* after this point we may be on a different stack, with the bp chain fixed */

		if (ret < 0)
		{
			mutex_lock(&thread_lock);
			free_ctx(child_ctx);
			mutex_unlock(&thread_lock);
		}
		else if (ret == 0)
			init_tls(child_ctx, sizeof(thread_ctx_t));
	}
	else
	{
		child_ctx = get_thread_ctx();
		ret = sys_clone(flags, 0, parent_tid, tls, child_tid);
		if (ret == 0)
			unshare_ctx(child_ctx);
	}

	if (ret == 0 && sp)
		child_ctx->user_esp = sp;

	return ret;
}

void user_exit(long status)
{
	mutex_lock(&thread_lock);
	free_ctx(get_thread_ctx());
	/* do not touch the scratch stack after releasing it */
	mutex_unlock_exit(status, &thread_lock);
}

long mutex_unlock_execve_or_die(char *filename, char *argv[], char *envp[], long *lock);
long sys_execve_or_die(char *filename, char *argv[], char *envp[])
{
	mutex_lock(&thread_lock);
	free_ctx(get_thread_ctx());
	/* do not touch the scratch stack after releasing it */
	mutex_unlock_execve_or_die(filename, argv, envp, &thread_lock);
}
