#ifndef TAINT_H
#define TAINT_H

/* all tainting is done pre-op */

int offset_mem(char *dst_mrm, char *src_mrm, long offset);

/* TAINT COPY */

int taint_copy_reg32_to_reg32(char *dest, int from_reg, int to_reg);
int taint_copy_reg16_to_reg16(char *dest, int from_reg, int to_reg);
int taint_copy_reg8_to_reg8(char *dest, int from_reg, int to_reg);

int taint_copy_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_mem16_to_reg16(char *dest, char *mrm, long offset);
int taint_copy_mem8_to_reg8(char *dest, char *mrm, long offset);

int taint_copy_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_copy_reg16_to_mem16(char *dest, char *mrm, long offset);
int taint_copy_reg8_to_mem8(char *dest, char *mrm, long offset);

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
int taint_copy_al_to_addr8(char *dest, long addr, long offset);

int taint_copy_addr32_to_eax(char *dest, long addr, long offset);
int taint_copy_addr16_to_ax(char *dest, long addr, long offset);
int taint_copy_addr8_to_al(char *dest, long addr, long offset);

int taint_copy_eax_to_str32(char *dest, long offset);
int taint_copy_ax_to_str16(char *dest, long offset);
int taint_copy_al_to_str8(char *dest, long offset);

int taint_copy_str32_to_eax(char *dest, long offset);
int taint_copy_str16_to_ax(char *dest, long offset);
int taint_copy_str8_to_al(char *dest, long offset);

int taint_copy_str32_to_str32(char *dest, long offset);
int taint_copy_str16_to_str16(char *dest, long offset);
int taint_copy_str8_to_str8(char *dest, long offset);

int taint_copy_popa32(char *dest, long offset);
int taint_copy_popa16(char *dest, long offset); /* XXX does not exempt esp from tainting */

int taint_copy_pusha32(char *dest, long offset);
int taint_copy_pusha16(char *dest, long offset);

/* TAINT COPYZX */

int taint_copy_reg16_to_reg32(char *dest, int from_reg, int to_reg);
int taint_copy_reg8_to_reg32(char *dest, int from_reg, int to_reg);
int taint_copy_reg8_to_reg16(char *dest, int from_reg, int to_reg);

int taint_copy_mem16_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_mem8_to_reg32(char *dest, char *mrm, long offset);
int taint_copy_mem8_to_reg16(char *dest, char *mrm, long offset);

/* TAINT OR */

int taint_or_reg32_to_reg32(char *dest, int from_reg, int to_reg);
int taint_or_reg16_to_reg16(char *dest, int from_reg, int to_reg);
int taint_or_reg8_to_reg8(char *dest, int from_reg, int to_reg);

int taint_or_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_or_reg16_to_mem16(char *dest, char *mrm, long offset);
int taint_or_reg8_to_mem8(char *dest, char *mrm, long offset);

int taint_or_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_or_mem16_to_reg16(char *dest, char *mrm, long offset);
int taint_or_mem8_to_reg8(char *dest, char *mrm, long offset);

int taint_xor_reg32_to_mem32(char *dest, char *mrm, long offset);
int taint_xor_reg16_to_mem16(char *dest, char *mrm, long offset);
int taint_xor_reg8_to_mem8(char *dest, char *mrm, long offset);

int taint_xor_mem32_to_reg32(char *dest, char *mrm, long offset);
int taint_xor_mem16_to_reg16(char *dest, char *mrm, long offset);
int taint_xor_mem8_to_reg8(char *dest, char *mrm, long offset);

/* TAINT ERASE */

int taint_erase_mem32(char *dest, char *mrm, long offset);
int taint_erase_mem16(char *dest, char *mrm, long offset);
int taint_erase_mem8(char *dest, char *mrm, long offset);

int taint_erase_reg32(char *dest, int reg);
int taint_erase_reg16(char *dest, int reg);
int taint_erase_reg8(char *dest, int reg);

int taint_erase_hireg16(char *dest, int reg);

int taint_erase_push32(char *dest, long offset);
int taint_erase_push16(char *dest, long offset);

int taint_erase_eax_edx(char *dest);
int taint_erase_ax_dx(char *dest);
int taint_erase_eax(char *dest);
int taint_erase_ax(char *dest);
int taint_erase_al(char *dest);

/* TAINT SWAP */

int taint_swap_reg32_reg32(char *dest, int reg1, int reg2);
int taint_swap_reg16_reg16(char *dest, int reg1, int reg2);
int taint_swap_reg8_reg8(char *dest, int reg1, int reg2);

int taint_swap_reg32_mem32(char *dest, char *mrm, long offset);
int taint_swap_reg16_mem16(char *dest, char *mrm, long offset);
int taint_swap_reg8_mem8(char *dest, char *mrm, long offset);

/* TAINT MISC */

int taint_leave32(char *dest, long offset);
int taint_leave16(char *dest, long offset); /* probablt erroneous */

#endif /* TAINT_H */

