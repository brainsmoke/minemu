#ifndef EXEC_H
#define EXEC_H

#include "load_elf.h"

int load_binary(elf_prog_t *prog);
long user_execve(char *filename, char *argv[], char *envp[]);

#endif /* EXEC_H */
