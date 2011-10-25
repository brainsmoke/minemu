
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

#ifndef SCRATCH_H
#define SCRATCH_H

#define JMP_CACHE_SIZE (0x10000)
#define JMP_CACHE_MEM_SIZE (JMP_CACHE_SIZE*8)

#ifndef __ASSEMBLER__

typedef struct
{
	char *addr;
	char *jit_addr;

} jmp_map_t;

extern jmp_map_t jmp_cache[JMP_CACHE_SIZE];

extern unsigned long sigwrap_stack[];
extern unsigned long sigwrap_stack_bottom[];
extern unsigned long scratch_stack[];
extern char jit_fragment_page[];

extern long jit_fragment_exit_eip;
extern long jit_fragment_restartsys;
extern long jit_fragment_running;
extern long jit_fragment_scratch;
extern long user_eip;
extern long jit_eip;
extern long sysenter_reentry;

extern long (*jit_return_addr)(void);
extern long (*runtime_ijmp_addr)(void);
extern long (*jit_fragment_exit_addr)(void);

extern long ijmp_taint;

#endif

#endif /* SCRATCH_H */
