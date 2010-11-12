#ifndef CODEMAP_H
#define CODEMAP_H

#include "jitmm.h"

typedef struct
{
    char *addr; 
    unsigned long len;
    char *jit_addr;
	unsigned long jit_len;

	/* mmapped file attributes */
	unsigned long long inode, dev;
	unsigned long mtime, pgoffset;

} code_map_t;

code_map_t *find_code_map(char *addr);
code_map_t *find_jit_code_map(char *jit_addr);

void add_code_region(char *addr, unsigned long len, unsigned long long inode,
                                                    unsigned long long dev,
                                                    unsigned long mtime,
                                                    unsigned long pgoffset);

void del_code_region(char *addr, unsigned long len);

#endif /* CODEMAP_H */
