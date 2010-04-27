#ifndef JITMM_H
#define JITMM_H

void jit_mm_init(void);

void jit_free(void *p);
void *jit_alloc(unsigned long size);
void *jit_realloc(void *p, unsigned long size);

#endif /* JITMM_H */
