#ifndef JMP_CACHE_H
#define JMP_CACHE_H

void add_jmp_mapping(char *addr, char *jit_addr);
char *find_jmp_mapping(char *addr);
void clear_jmp_mappings(char *addr, unsigned long len);

#define HASH_INDEX(addr) ((unsigned long)(addr)&0xfffful)

#endif /* JMP_CACHE_H */
