
#include <string.h>

#include "opcodes.h"

/* optable */

#define IL       0x00
#define OP       0x10
#define MODRM    0x20
#define ESCAPE   0x40
#define PREFIX   0x80

#define IMMB     0x01
#define IMMW     0x02
#define IMMA     0x04
#define IMMS     0x08

#define P1  (PREFIX|1)
#define P2  (PREFIX|2)
#define P3  (PREFIX|3)
#define P4  (PREFIX|4)
#define PREFIX_MASK (7)

#define O   (OP)
#define M   (MODRM)
#define I   (IL)
#define FP  (MODRM)

#define ESC (ESCAPE)
#define G38 (ESCAPE|1)
#define G3A (ESCAPE|2)
#define GF6 (ESCAPE|3)
#define GF7 (ESCAPE|4)
#define GFF (ESCAPE|5)

#define OB (OP|IMMB)
#define OW (OP|IMMW)
#define OL (OP|IMMW|IMMS)
#define OA (OP|IMMA)
#define OS (OP|IMMS)
#define OT (OP|IMMS|IMMB)

#define MB (MODRM|IMMB)
#define MW (MODRM|IMMW)
#define ML (MODRM|IMML)

static const unsigned char optable[] =
{
	[MAIN_OPTABLE] =
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  M , M , M , M , OB, OW, O , O , M , M , M , M , OB, OW, O , ESC,
/* 1? */  M , M , M , M , OB, OW, O , O , M , M , M , M , OB, OW, O , O ,
/* 2? */  M , M , M , M , OB, OW, P2, O , M , M , M , M , OB, OW, P2, O ,
/* 3? */  M , M , M , M , OB, OW, P2, O , M , M , M , M , OB, OW, P2, O ,
/* 4? */  O , O , O , O , O , O , O , O , O , O , O , O , O , O , O , O ,
/* 5? */  O , O , O , O , O , O , O , O , O , O , O , O , O , O , O , O ,
/* 6? */  O , O , I , I , P2, P2, P3, P4, OW, MW, OB, MB, I , I , I , I ,
/* 7? */  OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB, OB,
/* 8? */  MB, MW, MB, MB, M , M , M , M , M , M , M , M , M , M , M , M ,
/* 9? */  O , O , O , O , O , O , O , O , O , O , OL, O , O , O , O , O ,
/* A? */  OA, OA, OA, OA, O , O , O , O , OB, OW, O , O , O , O , O , O ,
/* B? */  OB, OB, OB, OB, OB, OB, OB, OB, OW, OW, OW, OW, OW, OW, OW, OW,
/* C? */  MB, MB, OS, O , M , M , MB, MW, OT, O , OS, O , O , OB, O , O ,
/* D? */  M , M , M , M , OB, OB, I , O , FP, FP, FP, FP, FP, FP, FP, FP,
/* E? */  OB, OB, OB, OB, OB, OB, OB, OB, OW, OW, OL, OB, O , O , O , O ,
/* F? */  P1, I , P1, P1, O , O , GF6,GF7,O , O , O , O , O , O , M , GFF,

	[ESC_OPTABLE] =
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  M , M , M , M , I , O , O , O , O , O , I , O , I , O , I , I ,
/* 1? */  M , M , M , M , M , M , M , M , M , I , I , I , I , I , I , M ,
/* 2? */  M , M , M , M , I , I , I , I , M , M , M , M , M , M , M , M ,
/* 3? */  O , O , O , O , O , O , I , O , G38,I , G3A,I , I , I , I , I ,
/* 4? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* 5? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* 6? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* 7? */  MB, MB, MB, MB, M , M , M , O , M , M , I , I , M , M , M , M ,
/* 8? */  OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW, OW,
/* 9? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* A? */  O , O , O , M , MB, M , I , I , O , O , O , M , MB, M , M , M ,
/* B? */  M , M , M , M , M , M , M , M , M , I , M , M , M , M , M , M ,
/* C? */  M , M , M , M , M , M , M , M , O , O , O , O , O , O , O , O ,
/* D? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* E? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , M ,
/* F? */  M , M , M , M , M , M , M , M , M , M , M , M , M , M , M , I ,

	[G38_OPTABLE] =
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  M , M , M , M , M , M , M , M , M , M , M , M , I , I , I , I ,
/* 1? */  M , I , I , I , M , M , I , M , I , I , I , I , M , M , M , I ,
/* 2? */  M , M , M , M , M , M , I , I , M , M , M , M , I , I , I , I ,
/* 3? */  M , M , M , M , M , M , I , M , M , M , M , M , M , M , M , M ,
/* 4? */  M , M , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 5? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 6? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 7? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 8? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 9? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* A? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* B? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* C? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* D? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* E? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* F? */  M , M , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,

	[G3A_OPTABLE] =
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  I , I , I , I , I , I , I , I , M , M , M , M , M , M , M , M ,
/* 1? */  I , I , I , I , M , M , M , M , I , I , I , I , I , I , I , I ,
/* 2? */  M , M , M , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 3? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 4? */  M , M , M , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 5? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 6? */  M , M , M , M , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 7? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 8? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* 9? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* A? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* B? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* C? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* D? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* E? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,
/* F? */  I , I , I , I , I , I , I , I , I , I , I , I , I , I , I , I ,

	[GF6_OPTABLE] =
          MB, I , M , M , M , M , M , M ,

	[GF7_OPTABLE] =
          MW, I , M , M , M , M , M , M ,

	[GFF_OPTABLE] =
          M , M , M , I , M , I , M , I ,

	[BAD_OP] = I,

	[CUTOFF_OP] = I,
};


static int read_modrm(const char *addr, instr_t *instr, int max_len)
{
	unsigned char mrm, sib;

	if (instr->len >= max_len)
		return instr->op = CUTOFF_OP;

	mrm = addr[instr->len];
	instr->len++;

	if ( (mrm & 0xC0) == 0xC0 )
		return 0;

	/* sib */
	if ( (mrm & 0x07) == 0x04 )
	{
		if (instr->len >= max_len)
			return instr->op = CUTOFF_OP;

		sib = addr[instr->len];
		instr->len += 1;

		if ( ((sib & 0x07) == 0x05) &&
		     ((mrm & 0xC0) == 0x00) )
			instr->len += 4;
	}

	if ( (mrm & 0xC7) == 0x05 )
		instr->len += 4;

	if ( (mrm & 0xC0) == 0x40 )
		instr->len += 1;

	if ( (mrm & 0xC0) == 0x80 )
		instr->len += 4;

	if (instr->len > max_len)
		return instr->op = CUTOFF_OP;

	return 0;
}

int read_op(char *addr, instr_t *instr, int max_len)
{
	int ret, type, op=MAIN_OPTABLE;

	*instr = (instr_t) { .addr=addr, .len=0 };

	while ( (instr->len < max_len) &&
	        ( (type = optable[(unsigned char)addr[instr->len]]) & PREFIX) )
	{
		if (instr->p[type & PREFIX_MASK])
			return instr->op = BAD_OP;

		instr->p[type & PREFIX_MASK] = addr[instr->len];
		instr->len++;
	}

	for(;;)
	{
		if (instr->len >= max_len)
			return instr->op = CUTOFF_OP;
		op += (unsigned char)addr[instr->len];
		type = optable[op];
		instr->len++;

		if (type >= ESC) switch (type)
		{
			case P1:
			case P2:
			case P3:
			case P4:
				return instr->op = BAD_OP;

			case ESC:
				op = ESC_OPTABLE;
				continue;

			case G38:
				op = G38_OPTABLE;
				continue;

			case G3A:
				op = G3A_OPTABLE;
				continue;

			case GF6:
			case GF7:
			case GFF:
				if (instr->len >= max_len)
					return instr->op = CUTOFF_OP;

				op = (type == GF6) ? GF6_OPTABLE :
				     (type == GF7) ? GF7_OPTABLE : GFF_OPTABLE;
				op += ( addr[instr->len] >> 3 ) & 0x7;
				type = optable[op];
			default:
				break;
		}
		break;
	}

	instr->mrm = instr->len;
	instr->op = op;

	if (type & MODRM)
	{
		ret = read_modrm(addr, instr, max_len);

		if (ret != 0)
			return ret;
	}

	instr->imm = instr->len;

	if (type & IMMW)
	{
		if (instr->p[3] == 0x66)
			instr->len += 2;
		else
			instr->len += 4;
	}

	if (type & IMMA)
	{
		if (instr->p[4] == 0x67)
			instr->len += 2;
		else
			instr->len += 4;
	}

	if (type & IMMS)
		instr->len += 2;

	if (type & IMMB)
		instr->len += 1;

	if (type == IL)
		return instr->op = BAD_OP;

	if (instr->len > max_len)
		return instr->op = CUTOFF_OP;

	return 0;
}

int op_size(char *addr, int max_len)
{
	instr_t instr;
	int ret = read_op(addr, &instr, max_len);
	return ret==CUTOFF_OP ? -1 : instr.len;
}

