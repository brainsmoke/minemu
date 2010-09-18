
#include <string.h>

#include "taint.h"

#include "error.h"
//#include "opcodes.h"
#include "jitcode.h"

/*       .____.____.____.____.
 * xmm5  |    scratch reg    |
 *       |____.____.____.____|
 * xmm6  | eax| ecx| edx| ebx|
 *       |____|____|____|____|
 * xmm7  | esp| ebp| esi| edi|
 *       |____|____|____|____|
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

int offset_mem(char *dst_mrm, char *src_mrm, long offset)
{
	int mrm = (unsigned char)src_mrm[0], sib=0;
	int disp32 = offset;
	int imm_index=1;

	if ( (mrm & 0xC0) == 0xC0 )
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

/* INSERTPS XMM <- MEM */
int taint_copy_mem32_to_reg32(char *dest, char *mrm, long offset)
{
	memcpy(dest, insertps, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg)<<4;
	return len;
}

/* EXTRD MEM <- XMM */
int taint_copy_reg32_to_mem32(char *dest, char *mrm, long offset)
{
	memcpy(dest, pextrd, 4);
	int len = 5+offset_mem(&dest[4], mrm, offset);
	int reg = ( dest[4] & 0x38 ) >> 3;
	dest[4] = ( dest[4] &~0x38 ) | ( taint_reg(reg)<<3 ); 
	dest[len-1] = taint_index(reg);
	return len;
}

/* INSERTPS XMM <- XMM */
int taint_copy_reg32_to_reg32(char *dest, int from_reg, int to_reg)
{
	memcpy(dest, insertps, 4);
	dest[4] = 0xC0 | ( taint_reg(to_reg)<<3 ) | taint_reg(from_reg); 
	dest[5] = taint_index(from_reg)<<6 | taint_index(to_reg)<<4;
	return 6;
}

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

/* MOVL MEM32 <- 0 */
int taint_clear_mem32(char *dest, char *mrm, long offset)
{
	dest[0] = '\xC7'; /* movl addr, imm32 */
	int len = 5+offset_mem(&dest[1], mrm, offset);
	dest[1] &=~ 0x38;
	memset(&dest[len-4], 0, 4);
	return len;
}

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


/* INSERTPS XMM <- MEM
 * POR XMM <- XMM
 * PEXTRD MEM <- XMM
 */
int taint_combine_reg32_to_mem32(char *dest, char *mrm, long offset)
{
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
	int len = scratch_load_mem32(dest, mrm, offset);
	memcpy(&dest[len], por, 3);
	dest[len+3] = 0xC0 | (taint_reg((mrm[0]&0x38)>>3)<<3) | scratch_reg();
	return len+4;
}

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

