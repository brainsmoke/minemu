
#include <string.h>

#include "taint.h"

#include "error.h"
//#include "opcodes.h"
#include "jit_code.h"

extern unsigned long taint_tmp[1];

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

static const char *pslldq_5         = "\x66\x0f\x73\xfd\x00",
                  *psrldq_5         = "\x66\x0f\x73\xdd\x00",
                  *blendw_5_to_6    = "\x66\x0f\x3a\x0e\xf5\x00",
                  *punpcklbw_6_to_5 = "\x66\x0f\x60\xee",
                  *punpckhbw_6_to_5 = "\x66\x0f\x68\xee",
                  *movapd_6_to_5    = "\x66\x0f\x28\xee",
                  *pinsrb           = "\x66\x0f\x3a\x20",
                  *pextrb           = "\x66\x0f\x3a\x14",
                  *pxor_5           = "\x66\x0f\xef\xed",
                  *insertps         = "\x66\x0f\x3a\x21",    /* insertps  */
                  *pextrd           = "\x66\x0f\x3a\x16",    /* pextrd    */
                  *por              = "\x66\x0f\xeb",        /* por       */
                  *palignr_6_to_5   = "\x66\x0f\x3a\x0f\xee",
                  *pmovzxbw_6_to_5  = "\x66\x0f\x38\x30\xee",
                  *pmovzxbd_6_to_5  = "\x66\x0f\x38\x31\xee",
                  *pmovzxbq_6_to_5  = "\x66\x0f\x38\x32\xee",
                  *pxorpunpckhbw_6_to_5 = "\x66\x0f\xef\xed\x66\x0f\x68\xee",
                  *por_6_to_5       = "\x66\x0f\xeb\xee";

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

/* in the same column as REG */
static int scratch_load_mem16(char *dest, char *mrm, long offset)
{
	memcpy(dest, "\x66\x0f\xc4", 3); /* pinsrw */
	int len = 4+offset_mem(&dest[3], mrm, offset);
	int reg = ( dest[3] & 0x38 ) >> 3;
	dest[3] = ( dest[3] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = taint_index(reg)<<1;
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

static int scratch_blend_reg32(char *dest, int reg)
{
	memcpy(dest, "\x66\x0f\x3a\x0e", 4); /* pblendw $indexbits,%xmm5,%reg */
	dest[4] = 0xC0 | ( taint_reg(reg)<<3 ) | scratch_reg();
	dest[5] = 3<<(taint_index(reg)<<1);
	return 6;
}

static int scratch_blend_reg16(char *dest, int reg)
{
	memcpy(dest, "\x66\x0f\x3a\x0e", 4); /* pblendw $indexbits,%xmm5,%reg */
	dest[4] = 0xC0 | ( taint_reg(reg)<<3 ) | scratch_reg();
	dest[5] = 1<<(taint_index(reg)<<1);
	return 6;
}

static int scratch_store_mem16(char *dest, char *mrm, long offset)
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

int taint_copy_reg8_to_reg32(char *dest, int from_reg, int to_reg)
{
	if ( taint_index(from_reg) == 0 )
	{
		/* pmovzxwd %reg, %xmm5 */
		memcpy(dest, "\x66\x0f\x38\x31\xe8", 5); 
		dest[4] |= taint_reg(from_reg);
		if (taint_index(to_reg))
			return 5+scratch_store_reg32(&dest[5], from_reg, to_reg);
		else
			return 5+scratch_blend_reg32(&dest[5], to_reg);
	}
	else
	{
		/* palignr $0x8,%reg,%xmm5 ; pmovzxwd %xmm5,%xmm5 */
		memcpy(dest, "\x66\x0f\x3a\x0f\xe8\x00\x66\x0f\x38\x31\xed", 11);
		dest[4] |= taint_reg(from_reg);
		dest[5] = taint_index(from_reg) * 4 - taint_index(to_reg);
		return 11+scratch_blend_reg32(&dest[11], to_reg);
	}
}

int taint_copy_mem16_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg16_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	int len = taint_erase_reg32(dest,(mrm[0]>>3)&0x7);
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
int taint_erase_reg32(char *dest, int reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( taint_reg(reg)<<3 ) | taint_reg(reg); 
	dest[5] = (taint_index(reg)<<6) |
	          (taint_index(reg)<<4) |
              (1<<taint_index(reg));
	return 6;
}

int taint_erase_reg16(char *dest, int reg)
{
	/* pxor   %xmm5,%xmm5 ; pblendw $0x1,%reg,%xmm5 */
	memcpy(dest, "\x66\x0f\xef\xed\x66\x0f\x3a\x0e\xc5", 9);
	dest[8] |= taint_reg(reg)<<3;
	dest[9] = 1<<(taint_index(reg)<<1);
	return 10;
}

int taint_erase_hireg16(char *dest, int reg)
{
	/* pxor   %xmm5,%xmm5 ; pblendw $0x1,%reg,%xmm5 */
	memcpy(dest, "\x66\x0f\xef\xed\x66\x0f\x3a\x0e\xc5", 9);
	dest[8] |= taint_reg(reg)<<3;
	dest[9] = 2<<(taint_index(reg)<<1);
	return 10;
}

/* MOVL MEM32 <- 0 */
int taint_erase_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_erase_reg32(dest, mrm[0]&0x7);

	dest[0] = '\xC7'; /* movl addr, imm32 */
	int len = 5+offset_mem(&dest[1], mrm, offset);
	dest[1] &=~ 0x38;
	memset(&dest[len-4], 0, 4);
	return len;
}

/* MOVW MEM16 <- 0 */
int taint_erase_mem16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_erase_reg16(dest, mrm[0]&0x7);

	dest[0] = '\x66';
	dest[1] = '\xC7'; /* movw addr, imm32 */
	int len = 4+offset_mem(&dest[2], mrm, offset);
	dest[2] &=~ 0x38;
	dest[len-2] = 0;
	dest[len-1] = 0;
	return len;
}

int taint_erase_eax(char *dest)
{
	return taint_erase_reg32(dest, 0);
}

int taint_erase_ax(char *dest)
{
	return taint_erase_reg16(dest, 0);
}

int taint_erase_al(char *dest)
{
	return taint_erase_reg8(dest, 0);
}

int taint_erase_eax_edx(char *dest)
{
	memcpy(dest, "\x66\x0f\x3a\x21\xf6\x05", 6);
	return 6;
}

int taint_erase_ax_dx(char *dest)
{
	/* pxor   %xmm5,%xmm5 ; pblendw $0x11,%xmm5,%xmm6 */
	memcpy(dest, "\x66\x0f\xef\xed\x66\x0f\x3a\x0e\xf5\x11", 10);
	return 10;
}

int taint_copy_popa32(char *dest, long offset)
{
	/* movdqu offset + 0(%esp),%xmm5
	 * movdqu offset +16(%esp),%xmm6
	 * pshufd      $0x1b,%xmm5,%xmm5
	 * pshufd      $0x1b,%xmm6,%xmm6
	 * pblendw     $0xfc,%xmm5,%xmm7
	 */
	memcpy(dest, "\xf3\x0f\x6f\xac\x24...."
	             "\xf3\x0f\x6f\xb4\x24...."
	             "\x66\x0f\x70\xed\x1b"
	             "\x66\x0f\x70\xf6\x1b"
	             "\x66\x0f\x3a\x0e\xfd\xfc", 34);

	imm_to(&dest[ 5], offset     );
	imm_to(&dest[14], offset+0x10);

	return 34;
}

int taint_copy_pusha32(char *dest, long offset)
{
	/* pshufd $0x1b,%xmm6,%xmm5
	 * movdqu %xmm5,0x10000000(%esp)
	 * pshufd $0x1b,%xmm7,%xmm5
	 * movdqu %xmm5,0x10000016(%esp)
	 */
	memcpy(dest, "\x66\x0f\x70\xee\x1b"
	             "\xf3\x0f\x7f\xac\x24...."
	             "\x66\x0f\x70\xef\x1b"
	             "\xf3\x0f\x7f\xac\x24....", 28);

	imm_to(&dest[10], offset-0x10);
	imm_to(&dest[24], offset-0x20);

	return 28;
}

static const char shuffle3[16] = { 6,7,14,15,4,5,12,13,2,3,10,11,0,1,8,9 };

int taint_copy_pusha16(char *dest, long offset)
{
	/* movdqu 0x10000000,%xmm5
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 * movq   %xmm6,0x10000000(%esp)
	 * movq   %xmm7,0x10000008(%esp)
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 */
	memcpy(dest, "\xf3\x0f\x6f\x2d...."
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd"
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd"
	             "\x66\x0f\xd6\xb4\x24...."
	             "\x66\x0f\xd6\xbc\x24...."
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd", 56);

	imm_to(&dest[ 4], (long)shuffle3);
	imm_to(&dest[33], offset-0x08);
	imm_to(&dest[42], offset-0x10);

	return 56;
}

int taint_copy_popa16(char *dest, long offset)
{
	/* movdqu 0x10000000,%xmm5
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 * movlpd 0x10000000(%esp),%xmm6
	 * movlpd 0x10000008(%esp),%xmm7
	 * pshufb %xmm5,%xmm6
	 * pshufb %xmm5,%xmm7
	 */
	memcpy(dest, "\xf3\x0f\x6f\x2d...."
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd"
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd"
	             "\x66\x0f\x12\xb4\x24...."
	             "\x66\x0f\x12\xbc\x24...."
	             "\x66\x0f\x38\x00\xf5"
	             "\x66\x0f\x38\x00\xfd", 56);

	imm_to(&dest[ 4], (long)shuffle3);
	imm_to(&dest[33], offset+0x08);
	imm_to(&dest[42], offset);

	return 56;
}

/* TAINT_COMBINE
 *
 */

/* INSERTPS XMM <- XMM (with clearing)
 * POR XMM <- XMM
 */
int taint_or_reg32_to_reg32(char *dest, int from_reg, int to_reg)
{
	int len = scratch_load_reg32(dest, from_reg, to_reg);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg(to_reg)<<3) | scratch_reg();
	return len+4;
}

int taint_or_reg16_to_reg16(char *dest, int from_reg, int to_reg)
{
	int len = scratch_load_reg32(dest, from_reg, to_reg);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (scratch_reg()<<3) | taint_reg(to_reg);
	return len+4+scratch_blend_reg16(&dest[len+4], to_reg);
}

/* INSERTPS XMM <- MEM
 * POR XMM <- XMM
 * PEXTRD MEM <- XMM
 */
int taint_or_reg32_to_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg32_to_reg32(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	int len = scratch_load_mem32(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (scratch_reg()<<3) | taint_reg((mrm[0]&0x38)>>3);
	len += 4 + scratch_store_mem32(&dest[len+4], mrm, offset);
	return len;
}

int taint_or_reg16_to_mem16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg16_to_reg16(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	int len = scratch_load_mem16(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (scratch_reg()<<3) | taint_reg((mrm[0]&0x38)>>3);
	len += 4 + scratch_store_mem16(&dest[len+4], mrm, offset);
	return len;
}

int taint_xor_reg32_to_mem32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) && ((mrm[0]>>3)&0x7) == (mrm[0]&0x7) )
		return taint_erase_reg32(dest, mrm[0]&0x7);
	else
		return taint_or_reg32_to_mem32(dest, mrm, offset);
}

int taint_xor_reg16_to_mem16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) && ((mrm[0]>>3)&0x7) == (mrm[0]&0x7) )
		return taint_erase_reg16(dest, mrm[0]&0x7);
	else
		return taint_or_reg16_to_mem16(dest, mrm, offset);
}


/* INSERTPS XMM <- MEM (with clearing)
 * POR XMM <- XMM
 */
int taint_or_mem32_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg32_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	int len = scratch_load_mem32(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg((mrm[0]&0x38)>>3)<<3) | scratch_reg();
	return len+4;
}

int taint_or_mem16_to_reg16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg16_to_reg16(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, pxor_5, 4);
	int len = 4+scratch_load_mem16(&dest[4], mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg((mrm[0]&0x38)>>3)<<3) | scratch_reg();
	return len+4;
}

int taint_xor_mem32_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) && ((mrm[0]>>3)&0x7) == (mrm[0]&0x7) )
		return taint_erase_reg32(dest, mrm[0]&0x7);
	else
		return taint_or_mem32_to_reg32(dest, mrm, offset);
}

int taint_xor_mem16_to_reg16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) && ((mrm[0]>>3)&0x7) == (mrm[0]&0x7) )
		return taint_erase_reg16(dest, mrm[0]&0x7);
	else
		return taint_or_mem16_to_reg16(dest, mrm, offset);
}

int taint_erase_push32(char *dest, long offset)
{
	memcpy(dest, "\xc7\x84\x24", 3);
	imm_to(&dest[3], offset-sizeof(long));
	imm_to(&dest[7], 0);
	return 11;
}

int taint_erase_push16(char *dest, long offset)
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

/* autogenerated table indicating which opcodes/immediates to use for byte copies
 *
 */
static const struct { char pre_shift, post_shift, upper_half; } bytecopy_table[8][8] =
{
/* src\dst     AL          CL          DL          BL          AH          CH          DH          BH     */
/* AL */  {{  0,  0,0},{  5, -6,0},{ -8,  7,0},{-12, 11,0},{  0,  0,0},{  5, -5,0},{ -8,  8,0},{-12, 12,0}},
/* CL */  {{  4, -9,0},{  0,  0,0},{ -4, -1,0},{ -8,  3,0},{  4, -8,0},{  0, -4,0},{ -4,  0,0},{ -8,  4,0}},
/* DL */  {{ -7, -2,0},{ -3, -6,0},{  0,  0,0},{  5,  2,1},{  8,  0,1},{ -3, -5,0},{  0,  8,1},{  5,  3,1}},
/* BL */  {{-11, -2,0},{ -7, -6,0},{  4, -1,1},{  0,  0,0},{-11, -1,0},{ -7, -5,0},{  4,  0,1},{  0,  4,1}},
/* AH */  {{  0, -2,0},{  4, -6,0},{ -7,  5,0},{-11,  9,0},{  0,  0,0},{  4, -5,0},{ -7,  6,0},{-11, 10,0}},
/* CH */  {{  5,-11,0},{  0, -6,0},{ -3, -3,0},{ -7,  1,0},{  5,-10,0},{  0,  0,0},{ -3, -2,0},{ -7,  2,0}},
/* DH */  {{ -8, -2,0},{ -4, -6,0},{  0,  6,1},{  4,  2,1},{ -8, -1,0},{ -4, -5,0},{  0,  0,0},{  4,  3,1}},
/* BH */  {{-12, -2,0},{ -8, -6,0},{  5, -3,1},{  0,  2,1},{-12, -1,0},{ -8, -5,0},{  5, -2,1},{  0,  0,0}},
};

static int reg8_index[] = { 0, 4, 8, 12, 1, 5, 9, 13 };

int shift_xmm5(char *dest, int c)
{
	if ( c == 0 )
		return 0;

	memcpy(dest, c>0 ? pslldq_5 : psrldq_5, 5);
	dest[4] = c > 0 ? c : -c;
	return 5;
}

int shift_xmm6_to_xmm5(char *dest, int c)
{
	if ( c < 0 )
	{
		/* palignr, save one op (yay!) */
		memcpy(dest, "\x66\x0f\x3a\x0f\xee", 6);
		dest[5] = -c;
		return 6;
	}
	else
	{
		memcpy(dest, movapd_6_to_5, 4);
		return 4+shift_xmm5(&dest[4], c);
	}
}

int taint_copy_reg8_to_reg8(char *dest, int from_reg, int to_reg)
{
	if ( from_reg == to_reg )
		return 0;

	int len = shift_xmm6_to_xmm5(dest, bytecopy_table[from_reg][to_reg].pre_shift);
	memcpy(&dest[len], bytecopy_table[from_reg][to_reg].upper_half ? punpckhbw_6_to_5 :
	                                                                 punpcklbw_6_to_5, 4);
	len += 4;
	len += shift_xmm5(&dest[len], bytecopy_table[from_reg][to_reg].post_shift);
	memcpy(&dest[len], blendw_5_to_6, 6);
	len += 6;
	dest[len-1] = 1<<(reg8_index[to_reg]/2);
	return len;
}

int taint_copy_reg8_to_mem8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg8_to_reg8(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	memcpy(dest, pextrb, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(0)<<3 ); 
	dest[len-1] = reg8_index[reg];
	return len;
}

int scratch_store_mem8(char *dest, char *mrm, long offset)
{
	memcpy(dest, pextrb, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = reg8_index[reg];
	return len;
}

int taint_copy_mem8_to_reg8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg8_to_reg8(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, pinsrb, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(0)<<3 ); 
	dest[len-1] = reg8_index[reg];
	return len;
}

static const struct { char strategy, pre_shift, post_shift; } byteor_table[8][8] =
{
/* src\dst       AL          CL          DL          BL          AH          CH          DH          BH   */
/*  AL  */ { {0, 0,  0}, {1, 0,  4}, {1, 0,  8}, {1, 0, 12}, {1, 0,  1}, {1, 0,  5}, {1, 0,  9}, {1, 0, 13} },
/*  CL  */ { {1, 0, -8}, {0, 0,  0}, {1, 0,  0}, {1, 0,  4}, {1, 0, -7}, {1, 0, -3}, {1, 0,  1}, {1, 0,  5} },
/*  DL  */ { {5, 0, -1}, {5, 0,  3}, {0, 0,  0}, {5, 0, 11}, {4, 8,  1}, {4, 8,  5}, {4, 8,  9}, {4, 8, 13} },
/*  BL  */ { {5, 0, -9}, {5, 0, -5}, {5, 0, -1}, {0, 0,  0}, {4,12,  1}, {4,12,  5}, {4,12,  9}, {4,12, 13} },
/*  AH  */ { {1, 0, -2}, {2, 0,  0}, {3, 0,  0}, {1, 0, 10}, {0, 0,  0}, {1, 0,  3}, {1, 0,  7}, {1, 0, 11} },
/*  CH  */ { {1, 0,-10}, {1, 0, -6}, {1, 0, -2}, {1, 0,  2}, {1, 0, -9}, {0, 0,  0}, {1, 0, -1}, {1, 0,  3} },
/*  DH  */ { {5, 0, -3}, {5, 0,  1}, {5, 0,  5}, {5, 0,  9}, {4, 9,  1}, {4, 9,  5}, {0, 0,  0}, {4, 9, 13} },
/*  BH  */ { {5, 0,-11}, {5, 0, -7}, {5, 0, -3}, {5, 0,  1}, {4,13,  1}, {4,13,  5}, {4,13,  9}, {0, 0,  0} },
};

int taint_or_reg8_to_reg8(char *dest, int from_reg, int to_reg)
{
	int len;

	switch ( byteor_table[from_reg][to_reg].strategy )
	{
		case 0:
			return 0;
		case 1:
			memcpy(dest, pmovzxbw_6_to_5, len=5);
			break;
		case 2:
			memcpy(dest, pmovzxbd_6_to_5, len=5);
			break;
		case 3:
			memcpy(dest, pmovzxbq_6_to_5, len=5);
			break;
		case 4:
			memcpy(dest, palignr_6_to_5, 5);
			dest[5] = byteor_table[from_reg][to_reg].pre_shift;
			len = 6;
			break;
		case 5:
			memcpy(dest, pxorpunpckhbw_6_to_5, len=8);
			break;
		default:
			return die("bad data in taint_or_reg8_to_reg8()");
	}

	len += shift_xmm5(&dest[len], byteor_table[from_reg][to_reg].post_shift);

	memcpy(&dest[len], por_6_to_5, 4);
	len += 4;
	memcpy(&dest[len], blendw_5_to_6, 6);
	len += 6;
	dest[len-1] = 1<<(reg8_index[to_reg]/2);
	return len;
}

static int scratch_load_mem8(char *dest, char *mrm, long offset)
{
	memcpy(dest, pinsrb, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = reg8_index[reg];
	return len;
}
 
int taint_or_reg8_to_mem8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg8_to_reg8(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	int len = scratch_load_mem8(dest, mrm, offset);
	memcpy(&dest[len], por_6_to_5, 4);
	len += 4 + scratch_store_mem8(&dest[len+4], mrm, offset);
	return len;
}

int taint_xor_reg8_to_mem8(char *dest, char *mrm, long offset)
{
	int reg = (mrm[0]>>3)&0x7;
	if ( !is_memop(mrm) && reg == (mrm[0]&0x7) )
		return taint_erase_reg8(dest, reg);
	else
		return taint_or_reg8_to_mem8(dest, mrm, offset);
}

int taint_or_mem8_to_reg8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_or_reg8_to_reg8(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, pxor_5, 4);
	int len = 4+scratch_load_mem8(&dest[4], mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg(0)<<3) | scratch_reg();
	return len+4;
}

int taint_xor_mem8_to_reg8(char *dest, char *mrm, long offset)
{
	int reg = (mrm[0]>>3)&0x7;
	if ( !is_memop(mrm) && reg == (mrm[0]&0x7) )
		return taint_erase_reg8(dest, reg);
	else
		return taint_or_mem8_to_reg8(dest, mrm, offset);
}


static const struct { char shift, upper_half; } byteerase_table[8] =
{
	/*  AL         CL         DL         BL         AH         CH         DH         BH  */
	{ -1, 0 }, { -5, 0 }, {  6, 1 }, {  2, 1 }, {  0, 0 }, { -4, 0 }, {  7, 1 }, {  3, 1 },
};

int taint_erase_reg8(char *dest, int reg)
{
	int len;
	if (byteerase_table[reg].upper_half)
	{
		memcpy(dest, pxor_5, 4);
		memcpy(&dest[4], punpckhbw_6_to_5, 4);
		len = 8;
	}
	else
		memcpy(dest, pmovzxbw_6_to_5, len=5);

	len += shift_xmm5(&dest[len], byteerase_table[reg].shift);
	memcpy(&dest[len], blendw_5_to_6, 6);
	len += 6;
	dest[len-1] = 1<<(reg8_index[reg]/2);
	return len;
}


int taint_erase_mem8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_erase_reg8(dest, mrm[0]&0x7);

	dest[0] = '\xC6'; /* movb addr, imm32 */
	int len = 2+offset_mem(&dest[1], mrm, offset);
	dest[1] &=~ 0x38;
	dest[len-1] = 0;
	return len;
}

/* blegh */
int taint_swap_reg8_reg8(char *dest, int reg1, int reg2)
{
	memcpy(dest, "\x66\x0f\x3a\x14\x35????\x00"
	             "\x66\x0f\x3a\x14\x35????\x00"
	             "\x66\x0f\x3a\x20\x35????\x00"
	             "\x66\x0f\x3a\x20\x35????\x00", 40);

	dest[ 9] = dest[29] = reg8_index[reg1];
	dest[19] = dest[39] = reg8_index[reg2];

	imm_to(&dest[ 5],   (long)taint_tmp);
	imm_to(&dest[15], 1+(long)taint_tmp);
	imm_to(&dest[25], 1+(long)taint_tmp);
	imm_to(&dest[35],   (long)taint_tmp);
	return 40;
}

int taint_swap_reg8_mem8(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_swap_reg8_reg8(dest, (mrm[0]>>3)&0x7, mrm[0]&0x7);

	memcpy(dest, movapd_6_to_5, 4);
	int len = 4+taint_copy_mem8_to_reg8(&dest[4], mrm, offset);
	return len+scratch_store_mem8(&dest[len], mrm, offset);
}

static const char byte2word_copy_table[8][4] =
{
	/* src\dst  AX  CX  DX  BX *
	 *          SP  BP  SI  DI */
	/* AL */ {  -1,  3,  7, 11 },
	/* CL */ {  -9, -5, -1,  3 },
	/* DL */ {  -1,  3,  7, 11 },
	/* BL */ {  -9, -5, -1,  3 },
	/* AH */ {  -3,  1,  5,  9 },
	/* CH */ { -11, -7, -3,  1 },
	/* DH */ {  -3,  1,  5,  9 },
	/* BH */ { -11, -7, -3,  1 },
};

int taint_copy_reg8_to_reg16(char *dest, int from_reg, int to_reg)
{
	memcpy(dest, pxor_5, 4);
	int len = 4;
	memcpy(&dest[len], reg8_index[from_reg]>7 ? punpckhbw_6_to_5 :
	                                            punpcklbw_6_to_5, 4);
	len += 4;
	len += shift_xmm5(&dest[len], byte2word_copy_table[from_reg][taint_index(to_reg)]);
	memcpy(&dest[len], blendw_5_to_6, 6);
	len += 6;
	dest[len-2] |= taint_reg(to_reg)<<3;
	dest[len-1] = 1<<(taint_index(to_reg)<<1);
	return len;
}

int taint_copy_mem8_to_reg16(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg8_to_reg16(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, pxor_5, 4);
	memcpy(&dest[4], pinsrb, 4);
	int len = 9+offset_mem(&dest[8], mrm, offset);
	int reg = ( dest[8] & 0x38 ) >> 3;
	dest[8] = ( dest[8] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = taint_index(reg)<<2;
	memcpy(&dest[len], "\x66\x0f\x3a\x0e\xc5", 5); /* pblendw $indexbit,%xmm5,%reg */
	dest[len+4] |= taint_reg(reg)<<3;
	dest[len+5] = 1<<(taint_index(reg)<<1);
	return len+6;
}

int taint_copy_mem8_to_reg32(char *dest, char *mrm, long offset)
{
	if ( !is_memop(mrm) )
		return taint_copy_reg8_to_reg32(dest, mrm[0]&0x7, (mrm[0]>>3)&0x7);

	memcpy(dest, pxor_5, 4);
	memcpy(&dest[4], pinsrb, 4);
	int len = 9+offset_mem(&dest[8], mrm, offset);
	int reg = ( dest[8] & 0x38 ) >> 3;
	dest[8] = ( dest[8] &~0x38 ) | ( scratch_reg()<<3 ); 
	dest[len-1] = taint_index(reg)<<2;
	memcpy(&dest[len], "\x66\x0f\x3a\x0e\xc5", 5); /* pblendw $indexbit,%xmm5,%reg */
	dest[len+4] |= taint_reg(reg)<<3;
	dest[len+5] = 3<<(taint_index(reg)<<1);
	return len+6;
}

int taint_copy_al_to_addr8(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x14\x35", 5); /* pextrb $0x0, %xmm6, addr+offset   */
	imm_to(&dest[5], addr+offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_addr8_to_al(char *dest, long addr, long offset)
{
	memcpy(dest,"\x66\x0f\x3a\x20\x35", 5); /* pinsrb $0x0, addr+offset, %xmm6 */
	imm_to(&dest[5], addr+offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_al_to_str8(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x14\xb7", 5); /* pextrb $0x0,%xmm6, offset(%edi)  */
	imm_to(&dest[5], offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_str8_to_al(char *dest, long offset)
{
	memcpy(dest, "\x66\x0f\x3a\x20\xb6", 5); /* pinsrb $0x0,offset(%esi),%xmm6 */
	imm_to(&dest[5], offset);
	dest[9] = '\x00';
	return 10;
}

int taint_copy_str8_to_str8(char *dest, long offset)
{
	/*
	 * pinsrb $0x0, offset(%esi), %xmm5
	 * pextrb $0x0, %xmm5, offset(%edi)
	 */
	memcpy(&dest[0], "\x66\x0f\x3a\x20\xae", 5);
	imm_to(&dest[5], offset);
	memcpy(&dest[9], "\x00\x66\x0f\x3a\x14\xaf", 6);
	imm_to(&dest[15], offset);
	dest[19] = '\x00';
	return 20;
}

