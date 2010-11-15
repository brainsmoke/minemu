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
