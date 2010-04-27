#ifndef CODEMAP_H
#define CODEMAP_H

#include "jitmm.h"

typedef struct
{
    char *addr; 
    unsigned long len;
    char *jit_addr;
	unsigned long jit_len;

} code_map_t;

code_map_t *find_code_map(char *addr);

void add_code_region(char *addr, unsigned long len);
void del_code_region(char *addr, unsigned long len);

#endif /* CODEMAP_H */
