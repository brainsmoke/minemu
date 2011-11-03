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

#ifndef THREAD_CTX_H
#define THREAD_CTX_H

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
	struct kernel_sigaction sigaction_list[KERNEL_NSIG];
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
};

inline thread_ctx_t *get_thread_ctx(void)
{
	return (thread_ctx_t *)get_tls_long(offsetof(thread_ctx_t, my_addr));
}

void protect_ctx(void);
void unprotect_ctx(void);

extern thread_ctx_t ctx[MAX_THREADS];

void init_thread_ctx(void);



#endif /* THREAD_CTX_H */
