#ifndef JMPCACHE_H
#define JMPCACHE_H

void add_jmp_mapping(char *addr, char *jit_addr);
char *find_jmp_mapping(char *addr);
void clear_jmp_mappings(char *addr, unsigned long len);
void move_jmp_mappings(char *jit_addr, unsigned long jit_len, char *new_addr);
char **alloc_jmp_cache(char *src_addr);
char **alloc_stub_cache(char *src_addr, char *jmp_addr, char *stub_addr);

void print_jmp_list(void);

#define HASH_INDEX(addr) ((unsigned long)(addr)&0xfffful)
#define HASH_OFFSET(i, addr) (((unsigned long)(i)-(unsigned long)(addr))&0xfffful)

#endif /* JMPCACHE_H */
