#ifndef TAINT_H
#define TAINT_H

int offset_mem(char *dst_mrm, char *src_mrm, long offset);

/* TAINT COPY */

int taint_copy_reg32_to_reg32(char *dest, int from_reg, int to_reg);
int taint_copy_reg16_to_reg16(char *dest, int from_reg, int to_reg);

int taint_copy_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_mem16_to_reg16(char *dest, char *mrm, long offset);

int taint_copy_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_copy_reg16_to_mem16(char *dest, char *mrm, long offset);

int taint_copy_push_reg32(char *dest, int reg, long offset);
int taint_copy_push_reg16(char *dest, int reg, long offset);

int taint_copy_push_mem32(char *dest, char *mrm, long offset);
int taint_copy_push_mem16(char *dest, char *mrm, long offset);

int taint_copy_pop_reg32(char *dest, int reg, long offset);
int taint_copy_pop_reg16(char *dest, int reg, long offset);

int taint_copy_pop_mem32(char *dest, char *mrm, long offset);
int taint_copy_pop_mem16(char *dest, char *mrm, long offset);

int taint_copy_eax_to_addr32(char *dest, long addr, long offset);
int taint_copy_ax_to_addr16(char *dest, long addr, long offset);

int taint_copy_addr32_to_eax(char *dest, long addr, long offset);
int taint_copy_addr16_to_ax(char *dest, long addr, long offset);

int taint_copy_eax_to_str32(char *dest, long offset);
int taint_copy_ax_to_str16(char *dest, long offset);

int taint_copy_str32_to_eax(char *dest, long offset);
int taint_copy_str16_to_ax(char *dest, long offset);

int taint_copy_str32_to_str32(char *dest, long offset);
int taint_copy_str16_to_str16(char *dest, long offset);

/* TAINT COPYZX */

int taint_copy_reg16_to_reg32(char *dest, int from_reg, int to_reg);
int taint_copy_reg8_to_reg16(char *dest, int from_reg, int to_reg);

int taint_copy_mem16_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_mem8_to_reg16(char *dest, char *mrm, long offset);

/* TAINT COMBINE */

int taint_combine_reg32_to_reg32(char *dest, int from_reg, int to_reg);
int taint_combine_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_combine_mem32_to_reg32(char *dest, char *mrm, long offset);

/* TAINT ERASE */

int taint_clear_mem32(char *dest, char *mrm, long offset);
int taint_clear_reg32(char *dest, int reg);
int taint_clear_mem16(char *dest, char *mrm, long offset);
int taint_clear_reg16(char *dest, int reg);
int taint_clear_hireg16(char *dest, int reg);
int taint_clear_push32(char *dest, long offset);
int taint_clear_push16(char *dest, long offset);

/* TAINT SWAP */

int taint_swap_reg32_reg32(char *dest, int reg1, int reg2);
int taint_swap_reg16_reg16(char *dest, int reg1, int reg2);

int taint_swap_reg32_mem32(char *dest, char *mrm, long offset);
int taint_swap_reg16_mem16(char *dest, char *mrm, long offset);


#endif /* TAINT_H */

