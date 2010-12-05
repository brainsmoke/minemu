
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
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

#ifndef LOAD_ELF_H
#define LOAD_ELF_H

#include <elf.h>

typedef struct
{
	Elf32_Ehdr hdr;
	Elf32_Phdr *phdr;
	unsigned long base, bss, brk;
	int fd;

} elf_bin_t;

typedef struct
{
	char *filename;
	char **argv, **envp;
	long *auxv;
	unsigned long task_size, stack_size;
	void *entry;
	long *sp;

	elf_bin_t bin;
	elf_bin_t interp;

} elf_prog_t;

int load_elf(elf_prog_t *prog);
int can_load_elf(elf_prog_t *prog);

int open_exec(const char *filename);

void set_aux(long *auxv, long id, long val);
long get_aux(long *auxv, long id);
long strings_count(char **s);


#endif /* LOAD_ELF_H */
