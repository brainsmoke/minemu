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

#ifndef THREADS_H
#define THREADS_H

#include <stddef.h>
#include "sigwrap.h"
#include "segments.h"

#define JMP_CACHE_SIZE (0x10000)
#define MAX_THREADS 32

typedef struct
{
	char *addr;
	char *jit_addr;

} jmp_map_t;


typedef long (*ijmp_t)(void);

typedef struct
{
	char fd_type[1024];
	long lock;

} file_ctx_t;

typedef struct
{
	struct kernel_sigaction sigaction_list[KERNEL_NSIG+1];
	long lock;

} sighandler_ctx_t;

typedef struct thread_ctx_s thread_ctx_t;

struct thread_ctx_s
{
	jmp_map_t jmp_cache[JMP_CACHE_SIZE];

	char fault_page0[0x1000];

	long sigwrap_stack[0x800];

	char jit_fragment_page[0x1000 - 3*sizeof(ijmp_t) -
	                                2*sizeof(long *) -
	                                  sizeof(thread_ctx_t *) -
	                                  sizeof(file_ctx_t *) -
	                                  sizeof(sighandler_ctx_t *) -
	                                  sizeof(stack_t)];
	ijmp_t jit_return_addr;                   /* normally */
	ijmp_t runtime_ijmp_addr;                 /*   read-  */
	ijmp_t jit_fragment_exit_addr;            /*   only   */
	long *sigwrap_stack_top;                  /*   for    */
	long *scratch_stack_top;                  /* catching */
	thread_ctx_t *my_addr;                    /*  stack-  */
	file_ctx_t *files;                        /* underrun */
	sighandler_ctx_t *sighandler;             /*   bugs   */
	stack_t altstack;                         /*    :-)   */

	long scratch_stack[0x2400 - 10 - sizeof(kernel_sigset_t)/sizeof(long)];

/* this */
	long user_esp; /* scratch_stack_top points here */
	long user_eip;
	long jit_eip;

	long jit_fragment_restartsys;
	long jit_fragment_running;
	long jit_fragment_entry;
	long jit_fragment_saved_esp;

	long ijmp_taint;
	long taint_tmp;
	long flags_tmp;

	kernel_sigset_t old_sigset;
/* gets copied in clone_relocate_stack() as well */
};

inline thread_ctx_t *get_thread_ctx(void)
{
	return (thread_ctx_t *)get_tls_long(offsetof(thread_ctx_t, my_addr));
}

void protect_ctx(void);
void unprotect_ctx(void);

void init_threads(void);

long user_clone(unsigned long flags, unsigned long sp, void *parent_tid, void *tls, void *child_tid);
void user_exit(long status);

long sys_execve_or_die(char *filename, char *argv[], char *envp[]);

void purge_caches(char *addr, unsigned long len);

void mutex_init(long *lock);
void mutex_lock(long *lock);
void mutex_unlock(long *lock);

void atomic_clear_8bytes(char *location, char *orig_val);

inline void commit(void)
{
	__asm__ __volatile__ ("mfence"::);
}

inline void siglock(thread_ctx_t *ctx)
{
	mutex_lock(&ctx->sighandler->lock);
}

inline void sigunlock(thread_ctx_t *ctx)
{
	mutex_unlock(&ctx->sighandler->lock);
}

#endif /* THREADS_H */
