#ifndef TAINT_H
#define TAINT_H

//#include "lib.h"
//#include "opcodes.h"

int is_memop(char *mrm);
int offset_mem(char *dst_mrm, char *src_mrm, long offset);

int taint_copy_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_copy_reg32_to_reg32(char *dest, int from_reg, int to_reg);

int taint_combine_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_combine_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_combine_reg32_to_reg32(char *dest, int from_reg, int to_reg);

int taint_clear_reg32(char *dest, int reg);
int taint_clear_mem32(char *dest, char *mrm, long offset);



#endif /* TAINT_H */

