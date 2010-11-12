#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#include "codemap.h"

void set_jit_cache_dir(const char *dir);
char *get_jit_cache_dir(void);

int try_load_jit_cache(code_map_t *map);
int try_save_jit_cache(code_map_t *map);

#endif /* JIT_CACHE_H */
