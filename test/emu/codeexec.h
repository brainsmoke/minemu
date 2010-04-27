#ifndef CODEEXEC_H
#define CODEEXEC_H

void codeexec(char *input, int input_sz, long *regs);

void save_fx(char *fx);
void load_fx(char *fx);

char *get_rwmem(size_t sz);

#endif /* CODEEXEC_H */
