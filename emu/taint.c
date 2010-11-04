
#include <string.h>

#include "taint.h"

#include "error.h"
//#include "opcodes.h"
#include "jit_code.h"

/*       .____.____.____.____.
 * xmm5  |    scratch reg    |
 *       |____.____.____.____|
 * xmm6  | eax| ecx| edx| ebx| --.
 *       |____|____|____|____|   }- taint_reg(reg) 
 * xmm7  | esp| ebp| esi| edi| --'
 *       |____|____|____|____|
 *          |    |    |    |
 *           ''''''|'''''''
 *          taint_index(reg)
 */

/*
 * INSERTP taint_index(reg), taint_reg(reg)|scratch_reg(), mem32
 * PEXTRD  taint_index(reg), taint_reg(reg)|scratch_reg(), mem32
 *
 */

static int taint_reg(int reg)
{
	return 0x06 | reg>>2;
}

static int taint_index(int reg)
{
	return reg & 0x03;
}

static int scratch_reg(void)
{
	return 0x05;
}

static int same_quadrant(int reg1, int reg2)
{
	return (reg1|1) == (reg2|1);
}

static int high_quadrant(int reg)
{
	return reg & 2;
}

static int is_memop(char *mrm)
{
	return (mrm[0] & 0xC0) != 0xC0;
}

int offset_mem(char *dst_mrm, char *src_mrm, long offset)
{
	int mrm = (unsigned char)src_mrm[0], sib=0;
	int disp32 = offset;
	int imm_index=1;

	if ( !is_memop(src_mrm) )
		die("'memory location' is a register");

	if ( (mrm & 0x07) == 0x04 )
	{
		imm_index=2;
		sib = (unsigned char)src_mrm[1];
		dst_mrm[1] = sib;
	}

	if ( ( (mrm & 0xC7) == 0x05 ) ||
	     ( (mrm & 0xC7) == 0x04 && (sib & 0x07) == 0x05 ) ||
 	     ( (mrm & 0xC0) == 0x80 ) )
	{
		disp32 += imm_at(&src_mrm[imm_index], 4);
	}
	else
	{
		if ( (mrm & 0xC0) == 0x40 )
			disp32 += (signed char)src_mrm[imm_index];

		mrm = 0x80 | (mrm & 0x3f);
	}

	dst_mrm[0] = mrm;
	imm_to(&dst_mrm[imm_index], disp32);
	return imm_index+sizeof(long);
}

static const char *insertps =
	"\x66\x0f\x3a\x21";    /* insertps  */

static const char *pextrd =
	"\x66\x0f\x3a\x16";    /* pextrd    */

static const char *por =
	"\x66\x0f\xeb";        /* por       */

/* in the same column as REG */
static int scratch_load_mem32(char *dest, char *mrm, long offset)
{
	memcpy(dest, insertps, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = (taint_index(reg)<<4) | ( (1<<taint_index(reg)) ^ 0xf );
	return len;
}

/* in the same column as dest_reg */
static int scratch_load_reg32(char *dest, int src_reg, int dest_reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( scratch_reg()<<3 ) | taint_reg(src_reg); 
	dest[5] = (taint_index(src_reg)<<6) |
	          (taint_index(dest_reg)<<4) |
              ((1<<taint_index(dest_reg)) ^ 0xf);
	return 6;
}

/* from the same column as src_reg */
static int scratch_store_reg32(char *dest, int src_reg, int dest_reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( taint_reg(dest_reg)<<3 ) | scratch_reg(); 
	dest[5] = (taint_index(src_reg)<<6) |
	          (taint_index(dest_reg)<<4);
	return 6;
}

int scratch_store_mem16(char *dest, char *mrm, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x15", 4); /* pextrw */
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = taint_index(reg)<<1;
	return len;
}

/* from the same column as REG */
static int scratch_store_mem32(char *dest, char *mrm, long offset)
{
	memcpy(dest, pextrd, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = taint_index(reg);
	return len;
}

static int direct_copy_mem32_to_mem32(char *dest, char *from_mrm, char *to_mrm, long offset)
{
	int len = scratch_load_mem32(dest, from_mrm, offset);
	dest[len-1] = 0; /* dword index 0 */
	len += scratch_store_mem32(&dest[len], to_mrm, offset);
	dest[len-1] = 0; /* dword index 0 */
	return len;
}

/* TAINT_COPY
 *
 */

/* INSERTPS XMM <- XMM */
int taint_copy_reg32_to_reg32(char *dest, int from_reg, int to_reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( taint_reg(to_reg)<<3 ) | taint_reg(from_reg); 
	dest[5] = taint_index(from_reg)<<6 | taint_index(to_reg)<<4;
	return 6;
}

/* INSRW XMM <- MEM */
int taint_copy_mem32_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg32_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, "\x66\x0f\x3a\x22", 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg);
	return len;
}

/* EXTRD MEM <- XMM */
int taint_copy_reg32_to_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg32_to_reg32(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	memcpy(dest, pextrd, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg);
	return len;
}

int taint_copy_push_reg32(char *dest, int reg, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x16\x84\x24", 6); /* pextrd $0x0, %xmm?, addr   */
	dest[4] |= taint_reg(reg)<<3;
	imm_to(&dest[6], offset-sizeof(long)); /* post-push esp + offset */
	dest[10] = taint_index(reg);
	return 11;
}

int taint_copy_push_mem32(char *dest, char *mrm, long offset)
{
	char mrm_stack[] = { 0x44, 0x24, '\xFC' };                       /* ...., -4(%esp) */
	return direct_copy_mem32_to_mem32(dest, mrm, mrm_stack, offset);
}

int taint_copy_pop_reg32(char *dest, int reg, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x22\x84\x24", 6); /* pinsrd $???, addr, %xmm6   */
	dest[4] |= taint_reg(reg)<<3;
	imm_to(&dest[6], offset);
	dest[10] = taint_index(reg);
	return 11;
}

int taint_copy_pop_mem32(char *dest, char *mrm, long offset)
{
	char mrm_stack[] = { 0x04, 0x24 };                               /* (%esp), ...    */
	return direct_copy_mem32_to_mem32(dest, mrm_stack, mrm, offset);
}

int taint_copy_eax_to_addr32(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x16\x35", 5); /* pextrd $0x0, %xmm6, addr+offset   */
	imm_to(&dest[5], addr+offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_ax_to_addr16(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x15\x35", 5); /* pextrw $0x0, %xmm6, addr+offset   */
	imm_to(&dest[5], addr+offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_addr32_to_eax(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x22\x35", 5); /* pinsrd $0x0, addr+offset, %xmm6 */
	imm_to(&dest[5], addr+offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_addr16_to_ax(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\xc4\x35", 4); /* pinsrw $0x0, addr+offset, %xmm6 */
	imm_to(&dest[4], addr+offset);
	dest[8] = '\x00';
	return 9;
}

int taint_copy_eax_to_str32(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x16\xb7", 5); /* pextrd $0x0,%xmm6, offset(%edi)  */
	imm_to(&dest[5], offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_ax_to_str16(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x15\xb7", 5); /* pextrw $0x0,%xmm6, offset(%edi)  */
	imm_to(&dest[5], offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_str32_to_eax(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x22\xb6", 5); /* pinsrds $0x0,offset(%esi),%xmm6 */
	imm_to(&dest[5], offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_str16_to_ax(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\xc4\xb6", 4); /* pinsrds $0x0,offset(%esi),%xmm6 */
	imm_to(&dest[4], offset);
	dest[8] = '\x00';
	return 9;
}

int taint_copy_str32_to_str32(char *dest, long offset)
{
	/*
	 * pinsrw  $0x0, offset(%esi), %xmm5
	 * pextrd  $0x0, %xmm5, offset(%edi)
	 */

	memcpy(&dest[0], "\x66\x0f\x3a\x21\xae", 5);
	imm_to(&dest[5], offset);
	memcpy(&dest[9], "\x00\x66\x0f\x3a\x16\xaf", 6);
	imm_to(&dest[15], offset);
	dest[19] = '\x00';
	return 20;
}

int taint_copy_str16_to_str16(char *dest, long offset)
{
	/*
	 * pinsrw $0x0, offset(%esi), %xmm5
	 * pextrw $0x0, %xmm5, offset(%edi)
	 */

	memcpy(&dest[0], "\x66\x0f\xc4\xae", 4);
	imm_to(&dest[4], offset);
	memcpy(&dest[8], "\x00\x66\x0f\x3a\x15\xaf", 6);
	imm_to(&dest[14], offset);
	dest[18] = '\x00';
	return 19;
}

int taint_copy_reg16_to_reg32(char *dest, int from_reg, int to_reg)
{
	int len;

	if (taint_index(from_reg)<2)
		/* pmovzxwd %reg, %xmm5 */
		memcpy(dest, "\x66\x0f\x38\x33\xe8", len=5); 
	else
		/* palignr $0x8,%reg,%xmm5 ; pmovzxwd %xmm5,%xmm5 */
		memcpy(dest, "\x66\x0f\x3a\x0f\xe8\x08\x66\x0f\x38\x33\xed", len=11);

	dest[4] |= taint_reg(from_reg);

	return len+scratch_store_reg32(&dest[len], from_reg * 2, to_reg);
}

int taint_copy_mem16_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg16_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	int len = taint_clear_reg32(dest,(mrm[0]>>3)&0x7);
	return len + taint_copy_mem16_to_reg16(&dest[len], mrm, offset);
}

int taint_copy_reg16_to_reg16(char *dest, int from_reg, int to_reg)
{
	if ( same_quadrant(from_reg, to_reg) )
	{
		if (from_reg == to_reg) /* not likely */
			return 0;

		if (high_quadrant(from_reg))
			memcpy(dest, "\xf3\x0f\x70\xc0", 4); /* pshufhw ...,%reg,%reg */
		else
			memcpy(dest, "\xf2\x0f\x70\xc0", 4); /* pshuflw ...,%reg,%reg */

		dest[3] |= (taint_reg(from_reg)<<3) | taint_reg(from_reg);
		dest[4] = (from_reg&0x1) ? '\xe6' : '\xc4';
		return 5;
	}

	int len = scratch_load_reg32(dest, from_reg, to_reg);
	memcpy(&dest[len], "\x66\x0f\x3a\x0e\xc5", 5); /* pblendw $indexbit,%xmm5,%reg */
	dest[len+4] |= taint_reg(to_reg)<<3;
	dest[len+5] = 1<<(taint_index(to_reg)<<1);
	return len+6;
}

int taint_copy_mem16_to_reg16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg16_to_reg16(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, "\x66\x0f\xc4", 3); /* pinsrw */
	int len = 4+offset_mem(&dest[3], mrm, offset);
	int reg = ( dest[3] & 0x38 ) >> 3;
	dest[3] = ( dest[3] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg)<<1;
	return len;
}

int taint_copy_reg16_to_mem16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg16_to_reg16(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	memcpy(dest, "\x66\x0f\x3a\x15", 4); /* pextrw */
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg)<<1;
	return len;
}

int taint_copy_push_reg16(char *dest, int reg, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x15\x84\x24", 6); /* pextrw $0x0, %xmm6, addr   */
	dest[4] |= taint_reg(reg)<<3;
	imm_to(&dest[6], offset-2); /* post-push esp + offset */
	dest[10] = taint_index(reg)<<1;
	return 11;
}

int taint_copy_push_mem16(char *dest, char *mrm, long offset)
{
	memcpy(dest, "\x66\x0f\xc4", 3); /* pinsrw $0, %xmm5, addr */
	int len = 4+offset_mem(&dest[3], mrm, offset);
	dest[3] = ( dest[3] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = 0;

	memcpy(&dest[len],"\x66\x0f\x3a\x15\xac\x24", 6); /* pextrw $0x0, %xmm5, addr   */
	imm_to(&dest[len+6], offset-2); /* post-push esp + offset */
	len += 11;
	dest[len-1] = 0;
	return len;
}

int taint_copy_pop_reg16(char *dest, int reg, long offset)
{
	char mrm[] = { '\x04'|(reg<<3), '\x24' };
	return taint_copy_mem16_to_reg16(dest, mrm, offset);
}

int taint_copy_pop_mem16(char *dest, char *mrm, long offset)
{
	memcpy(dest,"\x66\x0f\xc4\xac\x24", 5); /* pinsrw $0x0, %xmm5, addr   */
	imm_to(&dest[5], offset);
	dest[9] = 0;

	memcpy(&dest[10], "\x66\x0f\x3a\x15", 4); /* pextrw */
	int len = 15+offset_mem(&dest[14], mrm, offset);
	dest[14] = ( dest[14] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = 0;
	return len;
}

/* TAINT_CLEAR
 *
 */

/* INSERTPS XMM <- XMM (with clear) */
int taint_clear_reg32(char *dest, int reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( taint_reg(reg)<<3 ) | taint_reg(reg); 
	dest[5] = (taint_index(reg)<<6) |
	          (taint_index(reg)<<4) |
              (1<<taint_index(reg));
	return 6;
}

int taint_clear_reg16(char *dest, int reg)
{
	/* pxor   %xmm5,%xmm5 ; pblendw $0x1,%reg,%xmm5 */
	memcpy(dest, "\x66\x0f\xef\xed\x66\x0f\x3a\x0e\xc5", 9);
	dest[8] |= taint_reg(reg)<<3;
	dest[9] = 1<<(taint_index(reg)<<1);
	return 10;
}

int taint_clear_hireg16(char *dest, int reg)
{
	/* pxor   %xmm5,%xmm5 ; pblendw $0x1,%reg,%xmm5 */
	memcpy(dest, "\x66\x0f\xef\xed\x66\x0f\x3a\x0e\xc5", 9);
	dest[8] |= taint_reg(reg)<<3;
	dest[9] = 2<<(taint_index(reg)<<1);
	return 10;
}

/* MOVL MEM32 <- 0 */
int taint_clear_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_clear_reg32(dest, mrm[0]&0x7);

	dest[0] = '\xC7'; /* movl addr, imm32 */
	int len = 5+offset_mem(&dest[1], mrm, offset);
	dest[1] &=~ 0x38;
	memset(&dest[len-4], 0, 4);
	return len;
}

/* MOVW MEM16 <- 0 */
int taint_clear_mem16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_clear_reg16(dest, mrm[0]&0x7);

	dest[0] = '\x66';
	dest[1] = '\xC7'; /* movw addr, imm32 */
	int len = 4+offset_mem(&dest[2], mrm, offset);
	dest[2] &=~ 0x38;
	dest[len-2] = 0;
	dest[len-1] = 0;
	return len;
}

/* TAINT_COMBINE
 *
 */

/* INSERTPS XMM <- XMM (with clearing)
 * POR XMM <- XMM
 */
int taint_combine_reg32_to_reg32(char *dest, int from_reg, int to_reg)
{
	int len = scratch_load_reg32(dest, from_reg, to_reg);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg(to_reg)<<3) | scratch_reg();
	return len+4;
}

/* INSERTPS XMM <- MEM
 * POR XMM <- XMM
 * PEXTRD MEM <- XMM
 */
int taint_combine_reg32_to_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_combine_reg32_to_reg32(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	int len = scratch_load_mem32(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (scratch_reg()<<3) | taint_reg((mrm[0]&0x38)>>3);
	len += 4 + scratch_store_mem32(&dest[len+4], mrm, offset);
	return len;
}

/* INSERTPS XMM <- MEM (with clearing)
 * POR XMM <- XMM
 */
int taint_combine_mem32_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_combine_reg32_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	int len = scratch_load_mem32(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg((mrm[0]&0x38)>>3)<<3) | scratch_reg();
	return len+4;
}

int taint_clear_push32(char *dest, long offset)
{
	memcpy(dest, "\xc7\x84\x24", 3);
	imm_to(&dest[3], offset-sizeof(long));
	imm_to(&dest[7], 0);
	return 11;
}

int taint_clear_push16(char *dest, long offset)
{
	memcpy(dest, "\x66\xc7\x84\x24", 4);
	imm_to(&dest[4], offset-sizeof(short));
	dest[8]=dest[9]=0;
	return 10;
}

/* SWAP */

int taint_swap_reg32_reg32(char *dest, int reg1, int reg2)
{
	if (taint_reg(reg1) == taint_reg(reg2))
	{
		memcpy(dest, "\x66\x0f\x70\xc0", 4); /* pshufd */
		dest[3] |= taint_reg(reg1)*9;
		int i1=taint_index(reg1)*2, i2=taint_index(reg2)*2;
		/* byte enocded 2 bit to 2 bit function swapping reg1 and reg2 words */
		dest[4] = 0xe4  + ( ( ((i2-i1)<<i1) + ((i1-i2)<<i2) ) >> 1);
		return 5;
	}
	int len = scratch_load_reg32(dest, reg1, reg2);
	len += taint_copy_reg32_to_reg32(&dest[len], reg2, reg1);
	return len + scratch_store_reg32(&dest[len], reg2, reg2);
}

/* OMG */
int taint_swap_reg16_reg16(char *dest, int reg1, int reg2)
{
	if (same_quadrant(reg1, reg2))
	{
		if (reg1 == reg2)
			return 0;

		if (high_quadrant(reg1))
			memcpy(dest, "\xf3\x0f\x70\xc0\xc6", 5); /* pshufhw $0xc6,%reg,%reg */
		else
			memcpy(dest, "\xf2\x0f\x70\xc0\xc6", 5); /* pshuflw $0xc6,%reg,%reg */

		dest[3] |= taint_reg(reg1)*9;
		return 5;
	}
	else if (taint_reg(reg1) == taint_reg(reg2))
	{
		memcpy(dest, "\x66\x0f\x70\xc0", 4); /* pshufd */
		dest[3] |= scratch_reg()<<3 | taint_reg(reg1);
		int i1=taint_index(reg1)*2, i2=taint_index(reg2)*2;
		/* byte enocded 2 bit to 2 bit function swapping reg1 and reg2 words */
		dest[4] = 0xe4  + ( ( ((i2-i1)<<i1) + ((i1-i2)<<i2) ) >> 1);

		memcpy(&dest[5], "\x66\x0f\x3a\x0e\xc5", 5); /* pblendw $indices,%xmm5,%reg */
		dest[9] |= taint_reg(reg1)<<3;
		dest[10] = (1<<i1) | (1<<i2);
		return 11;
	}
	else if (taint_index(reg1) != taint_index(reg2))
	{
		memcpy(dest, "\x66\x0f\x3a\x21\xe8\x00" /* insertps */
		             "\x66\x0f\x3a\x21\xe8\x00"
		             "\x66\x0f\x3a\x0e\xc5\x00" /* blendw   */
		             "\x66\x0f\x3a\x0e\xc5\x00", 24);
		dest[4]  |= taint_reg(reg1);
		dest[5]  |= (taint_index(reg1)<<6) | (taint_index(reg2)<<4);
		dest[10] |= taint_reg(reg2);
		dest[11] |= (taint_index(reg2)<<6) | (taint_index(reg1)<<4);
		dest[16] |= taint_reg(reg1)<<3;
		dest[17] |= 1<<(taint_index(reg1)<<1);
		dest[22] |= taint_reg(reg2)<<3;
		dest[23] |= 1<<(taint_index(reg2)<<1);
		return 24;
	}
	else
	{
		int tmp_index = taint_index(reg1) ^ 1;
		memcpy(dest, "\x66\x0f\x28\xee"          /* movapd           %xmm6, %xmm5 */
		             "\x66\x0f\x3a\x21\xef\x00"  /* insertps   ...,  %xmm7, %xmm5 */
		             "\xf2\x0f\x70\xed\xc6"      /* pshuf[lh]w $0xc6,%xmm5, %xmm5 */
		             "\x66\x0f\x3a\x0e\xf5\x00"  /* blendw     ...., %xmm5, %xmm7 */
		             "\x66\x0f\x3a\x21\xfd\x00", /* insertps   ...., %xmm5, %xmm6 */
		             27);

		if (high_quadrant(reg1))
			dest[10] = '\xf3'; /* pshufhw */

		dest[9] = (taint_index(reg1)<<6) | (tmp_index<<4);
		dest[20] = 1<<(taint_index(reg1)<<1);
		dest[26] = (tmp_index<<6) | (taint_index(reg1)<<4);
		return 27;
	}
}

int taint_swap_reg32_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_swap_reg32_reg32(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	int len = scratch_load_mem32(dest, mrm, offset);
	len += taint_copy_reg32_to_mem32(&dest[len], mrm, offset);
	return len + scratch_store_reg32(&dest[len], (mrm[0]>>3)&0x7, (mrm[0]>>3)&0x7);
}

int taint_swap_reg16_mem16(char *dest, char *mrm, long offset)
{
	int reg = (mrm[0]>>3)&0x7;
	if ( !is_memop(mrm) )
		return taint_swap_reg16_reg16(dest, reg, mrm[0]&0x7);

	int len = scratch_load_reg32(dest, reg, reg);
	len += taint_copy_mem16_to_reg16(&dest[len], mrm, offset);
	return len + scratch_store_mem16(&dest[len], mrm, offset);
}


