#ifndef DEBUG_H
#define DEBUG_H

void dump_stack(long stack_end);
void dump_stack_rev(long stack_end);

void dump_mem(void *mem, long len);

#endif /* DEBUG_H */
