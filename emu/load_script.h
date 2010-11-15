#ifndef LOAD_SCRIPT_H
#define LOAD_SCRIPT_H

#include "load_elf.h"

int load_script(elf_prog_t *prog);
int can_load_script(elf_prog_t *prog);

#endif /* LOAD_SCRIPT_H */
