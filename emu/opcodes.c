
#include <string.h>

#include "opcodes.h"
#include "jmpcache.h"
#include "scratch.h"
#include "runtime.h"
#include "error.h"
#include "taint.h"

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

#define BATSHIT  0xFF /* insane exception */

#define P1  (PREFIX|0)
#define P2  (PREFIX|1)
#define P3  (PREFIX|2)
#define P4  (PREFIX|3)

#define O   (OP)
#define M   (MODRM)
#define I   (IL)
#define FP  (MODRM)

#define ESC (ESCAPE)
#define G38 (ESCAPE|1)
#define G3A (ESCAPE|2)

#define OB (OP|IMMB)
#define OW (OP|IMMW)
#define OL (OP|IMMW|IMMS)
#define OA (OP|IMMA)
#define OS (OP|IMMS)
#define OT (OP|IMMS|IMMB)

#define MB (MODRM|IMMB)
#define MW (MODRM|IMMW)
#define ML (MODRM|IMML)

#define BAT (BATSHIT)

static const unsigned char main_optable[] =
{
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
/* F? */  P1, I , P1, P1, O , O , BAT,BAT,O , O , O , O , O , O , M , M ,
};

static const unsigned char esc_optable[] =
{
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
};

static const unsigned char g38_optable[] =
{
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
};

static const unsigned char g3A_optable[] =
{
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
};

/* op action */

#define C  (0)

#define U  (1)

#define CONTROL      (0x20)
#define CONTROL_MASK (~(CONTROL-1))

#define JR (CONTROL|0)
#define JC (CONTROL|1)
#define JF (CONTROL|2)
#define JI (CONTROL|3)

#define L  (CONTROL|4)

#define CR (CONTROL|5)
#define CF /*(CONTROL|6)*/ U
#define CI (CONTROL|7)

#define R  (CONTROL|8)
#define RC (CONTROL|9)
#define RF /*(CONTROL|10)*/ U

#define IA (CONTROL|11)

#define JOIN (CONTROL|12)

#define INT (0x40)
#define SE  (0x41)

#define XXX (C) /* todo */
#define IGN (C) /* todo */

#define TAINT (0x80)
#define TAINT_MASK (~(TAINT-1))
#define TCM   (TAINT|0) /* copy taint:   reg -> mem */
#define TCR   (TAINT|1) /*               mem -> reg */
#define TEM   (TAINT|2) /* erase taint:  mem        */
#define TER   (TAINT|3) /*               reg        */
#define TOM   (TAINT|4) /* or taint:     reg -> mem */
#define TOR   (TAINT|5) /* or taint:     mem -> reg */
//#define TCA   (C)
//#define TEA   (C)
//#define TOA   (C)
#define BCA   (C)
//#define BEA   (C)
//#define BOA   (C)

#define TCO   (C)
#define TCO   (C)
//#define TEO   (C)
//#define TOO   (C)
#define BCO   (C)
//#define BEO   (C)
//#define BOO   (C)

#define BCM   (TAINT|6)  /* copy taint:   reg -> mem */
#define BCR   (TAINT|7)  /*               mem -> reg */
#define BEM   (TAINT|8)  /* erase taint:  mem        */
#define BER   (TAINT|9)  /*               reg        */
#define BOM   (TAINT|10) /* or taint:     reg -> mem */
#define BOR   (TAINT|11) /* or taint:     mem -> reg */

#define TXM   (TAINT|12) /* or taint:     reg -> mem / clear if src==dest */
#define TXR   (TAINT|13) /* or taint:     mem -> reg / clear if src==dest */
#define BXM   (TAINT|12) /* or taint:     reg -> mem / clear if src==dest */
#define BXR   (TAINT|13) /* or taint:     mem -> reg / clear if src==dest */

#define TPR   (TAINT|14) /* copy taint  for push     */
#define TPE   (TAINT|15) /* clear taint for push     */
#define TPM   (TAINT|16) /* clear taint for push     */
#define TQR   (TAINT|17) /* propagate taint for pop  */
#define TQE   (TAINT|18) /* propagate taint for pop  */
#define TQM   (TAINT|19) /* propagate taint for pop  */

#define BMI   (TAINT|20) /* taint multiply with immediate */
#define TMI   (TAINT|21) /* taint multiply with immediate */
#define BMM   (TAINT|22) /* taint multiply */
#define TMM   (TAINT|23) /* taint multiply */
#define BMR   (TAINT|24) /* taint multiply */
#define TMR   (TAINT|25) /* taint multiply */

#define BSW   (TAINT|26)
#define TSW   (TAINT|27)

#define BSA   (TAINT|28)
#define TSA   (TAINT|29)

#define TCH   (TAINT|30)
#define TCD   (TAINT|31)

#define CMR   (TAINT|32)
#define CMM   (TAINT|33)

static const unsigned char main_action[] =

{
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */ BOM,TOM,BOR,TOR, C , C , C , C ,BOM,TOM,BOR,TOR, C , C , C , C ,
/* 1? */ BOM,TOM,BOR,TOR, C , C , C , C ,BXM,TXM,BXR,TXR, C , C , C , C ,
/* 2? */ BOM,TOM,BOR,TOR, C , C , C , C ,BXM,TXM,BXR,TXR, C , C , C , C ,
/* 3? */ BXM,TXM,BXR,TXR, C , C , C , C , C , C , C , C , C , C , C , C ,
/* 4? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 5? */ TPR,TPR,TPR,TPR,TPR,TPR,TPR,TPR,TQR,TQR,TQR,TQR,TQR,TQR,TQR,TQR,
/* 6? */ IGN,IGN, C , C , C , C , C , C ,TPE,IGN,TPE,IGN,IGN,IGN,IGN,IGN,
/* 7? */  JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC,
/* 8? */  C , C , C , C , C , C ,BSW,TSW,BCM,TCM,BCR,TCR,IGN,IGN,IGN,IGN,
/* 9? */  C ,TSA,TSA,TSA,TSA,TSA,TSA,TSA,TCH,TCD, CF, C ,TPE, C , C ,BCA,
/* A? */ BCO,TCO, C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* B? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* C? */  C , C , RC, R , C , C , C , C , C , C , RF, RF, C ,INT, C , C ,
/* D? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* E? */  L , L , L , L , C , C , C , C , CR, JR, JF, JR, C , C , C , C ,
/* F? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , IA,
};
static const unsigned char esc_action[] =
{
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  C , C , C , C , C , U , C , U , C , C , C , C , C , C , C , C ,
/* 1? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 2? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 3? */  C , C , C , C , SE, C , C , C , C , C , C , C , C , C , C , C ,
/* 4? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 5? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 6? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 7? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 8? */  JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC, JC,
/* 9? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* A? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* B? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* C? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* D? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* E? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* F? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
};
static const unsigned char g38_action[] =
{
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 1? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 2? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 3? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 4? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 5? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 6? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 7? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 8? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 9? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* A? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* B? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* C? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* D? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* E? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* F? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
};
static const unsigned char g3A_action[] =
{
/*        ?0  ?1  ?2  ?3  ?4  ?5  ?6  ?7  ?8  ?9  ?A  ?B  ?C  ?D  ?E  ?F */
/* 0? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 1? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 2? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 3? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 4? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 5? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 6? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 7? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 8? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* 9? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* A? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* B? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* C? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* D? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* E? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
/* F? */  C , C , C , C , C , C , C , C , C , C , C , C , C , C , C , C ,
};

static int read_modrm(const char *addr, instr_t *instr, int max_len)
{
	unsigned char mrm, sib;

	if (instr->len >= max_len)
	{
		instr->action = JOIN;
		return CODE_JOIN;
	}

	mrm = addr[instr->len];
	instr->len++;

	if ( (mrm & 0xC0) == 0xC0 )
		return 0;

	/* sib */
	if ( (mrm & 0x07) == 0x04 )
	{
		if (instr->len >= max_len)
		{
			instr->action = JOIN;
			return CODE_JOIN;
		}

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
	{
		instr->action = JOIN;
		return CODE_JOIN;
	}

	return 0;
}

int read_op(char *addr, instr_t *instr, int max_len)
{
	const unsigned char *optable = main_optable, *action = main_action;
	int ret, type, code;

	*instr = (instr_t) { .addr=addr, .len=0 };

	for(;;)
	{
		if (instr->len >= max_len)
		{
			instr->action = JOIN;
			return CODE_JOIN;
		}

		code = (unsigned char)addr[instr->len];
		type = optable[code];
		instr->len++;

		switch (type)
		{
			case P1: case P2: case P3: case P4:

				if (instr->p[type-PREFIX] != 0)
				{
					instr->action = U;
					return CODE_ERR;
				}

				instr->p[type-PREFIX] = code;
				continue;

			case ESC:
				optable = esc_optable;
				action = esc_action;
				continue;
			case G38:
				optable = g38_optable;
				action = g38_action;
				continue;
			case G3A:
				optable = g3A_optable;
				action = g3A_action;
				continue;

			case BAT:
				if (instr->len >= max_len)
				{
					instr->action = JOIN;
					return CODE_JOIN;
				}

				if ( (addr[instr->len] & 0x38) == 0x00 )
					type = (code == 0xF6) ? MB : MW;
				else if ( (addr[instr->len] & 0x38) == 0x01 )
					type = I;
				else
					type = M;

			default:
				break;
		}
		break;
	}

	instr->type = type;
	instr->mrm = instr->len;
	instr->action = action[code];

	if (instr->action == IA)
	{
		if (instr->len >= max_len)
		{
			instr->action = JOIN;
			return CODE_JOIN;
		}

		switch (addr[instr->len] & 0x38)
		{
			case 0x10: instr->action = CI; break; /* indirect call */
			case 0x18: instr->action = U;  break;  /* indirect far call */
			case 0x20: instr->action = JI; break; /* indirect jump */
			case 0x28: instr->action = U;  break;  /* indirect far jump */
			default:
				instr->action = C;
		}

		if (instr->p[P4-PREFIX] == 0x67)
			instr->action = U;
	}

	if (type & MODRM)
	{
		ret = read_modrm(addr, instr, max_len);

		if (ret != 0)
			return ret;
	}

	instr->imm = instr->len;

	if (type & IMMW)
	{
		if (instr->p[P3-PREFIX] == 0x66)
			instr->len += 2;
		else
			instr->len += 4;
	}

	if (type & IMMA)
	{
		if (instr->p[P4-PREFIX] == 0x67)
			instr->len += 2;
		else
			instr->len += 4;
	}

	if (type & IMMS)
		instr->len += 2;

	if (type & IMMB)
		instr->len += 1;

	if (type == IL)
		instr->action = U;

	if (instr->len > max_len)
		instr->action = JOIN;

	switch (instr->action)
	{
		case JOIN:
			return CODE_JOIN;
		case U:
			return CODE_ERR;
		case JR: case JF: case JI: case R: case RC:
			return CODE_STOP;
		default:
			return 0;
	}
}

int op_size(char *addr, int max_len)
{
	instr_t instr;
	int ret = read_op(addr, &instr, max_len);
	return ret<0 ? ret : instr.len;
}

long imm_at(char *addr, long size)
{
	long imm=0;
	memcpy(&imm, addr, size);
	if (size == 1)
		return *(signed char*)&imm;
	else
		return imm;
}

void imm_to(char *dest, long imm)
{
	memcpy(dest, &imm, sizeof(long));
}

int generate_jump(char *dest, char *jmp_addr, trans_t *trans,
                  char *map, unsigned long map_len)
{
	if (contains(map, map_len, jmp_addr))
	{
		dest[0] = '\xE9'; /* jmp i32 */
		*trans = (trans_t){ .jmp_addr=jmp_addr, .imm=1, .len=5 };
	}
	else
	{
		int stub_len = generate_stub(&dest[6], jmp_addr, &dest[2]);

		dest[0] = '\xFF'; /* jmp *mem */
		dest[1] = '\x25';

		*trans = (trans_t){ .len=6+stub_len };
	}

	return trans->len;
}

static int generate_jcc(char *dest, char *jmp_addr, int cond, trans_t *trans,
                        char *map, unsigned long map_len)
{
	if (contains(map, map_len, jmp_addr))
	{
		dest[0] = '\x0F'; /* jcc i32 */
		dest[1] = '\x80'+cond;
		*trans = (trans_t){ .jmp_addr=jmp_addr, .imm=2, .len=6 };
	}
	else
	{
		int stub_len = generate_stub(&dest[8], jmp_addr, &dest[4]);

		dest[0] = '\x70'+ (cond^1); /* j!cc over( jmp *mem ) */
		dest[1] = '\x06'+stub_len;
		dest[2] = '\xFF';
		dest[3] = '\x25';

		*trans = (trans_t){ .len=stub_len+8 };
	}

	return trans->len;
}

static const char *ijump_head  =
	"\xA3????"         /* mov %eax, scratch_stack-4 */
;

static const char *ijump_tail =
	"\x89\x25????"     /* mov %esp, scratch_stack   */
	"\xBC????"         /* mov $scratch_stack-4 %esp */
	"\x9c"             /* pushf                     */
	"\x3B\x05????"     /* cmp o_addr, %eax          */
	"\x2E\x75\x09"     /* jne, predict hit          */
	"\x9d"             /* popf                      */
	"\x58"             /* pop %eax                  */
	"\x5C"             /* pop %esp                  */
	"\xFF\x25????"     /* jmp *j_addr               */
	"\xA3????"         /* mov %eax, o_addr          */
	"\x68????"         /* push j_addr               */
	"\xFF\x25????"     /* jmp *runtime_ijmp_addr    */
 ;                         

static int generate_ijump_tail(char *dest, char **jmp_orig, char **jmp_jit)
{
	memcpy(&dest[0x00], ijump_tail, 0x2E);

	imm_to(&dest[0x02], (long)&scratch_stack);
	imm_to(&dest[0x07], (long)&scratch_stack-4);
	imm_to(&dest[0x0E], (long)jmp_orig);
	imm_to(&dest[0x1A], (long)jmp_jit);
	imm_to(&dest[0x1F], (long)jmp_orig);
	imm_to(&dest[0x24], (long)jmp_jit);
	imm_to(&dest[0x2A], (long)&runtime_ijmp_addr);

	return 0x2E;
}

static int generate_ijump(char *dest, instr_t *instr, trans_t *trans)
{
	char **cache = alloc_jmp_cache(instr->addr);
	long mrm_len = instr->len - instr->mrm;
	*trans = (trans_t){ .len=0x05 };

	memcpy(dest, ijump_head, 0x05);
	imm_to(&dest[0x01], (long)&scratch_stack-4);

	if (instr->p[P2-PREFIX]) /* needed for call *%gs:0x10, sysenter mechanism */
	{
		dest[trans->len] = instr->p[P2-PREFIX];
		trans->len++;
	}
	dest[trans->len] = '\x8B'; /* mov ... ( -> %eax )       */
	trans->len++;

	memcpy(&dest[trans->len], &instr->addr[instr->mrm], mrm_len);
	dest[trans->len] &= 0xC7; /* -> %eax */
	trans->len += mrm_len;
	trans->len += generate_ijump_tail(&dest[trans->len], &cache[0], &cache[1]);

	return trans->len;
}

/* XXX MAKES CODE IMMOBILE */
static const char *call_head =
	"\x68????"         /* push $retaddr */
	"\xc7\x05????????" /* movl $addr, jmp_list.addr[HASH_INDEX(addr)] */
	"\xc7\x05????????" /* movl $jit_addr, jmp_list.jit_addr[HASH_INDEX(addr)] */
;

static int generate_call_head(char *dest, instr_t *instr, trans_t *trans)
{
	int hash = HASH_INDEX(&instr->addr[instr->len]);
	memcpy(dest, call_head, 0x19);
	imm_to(&dest[0x01], (long)&instr->addr[instr->len]);
	imm_to(&dest[0x07], (long)&jmp_list.addr[hash]);
	imm_to(&dest[0x0B], (long)&instr->addr[instr->len]);
	imm_to(&dest[0x11], (long)&jmp_list.jit_addr[hash]);
/* XXX FUGLY translated address is inserted by caller since we don't know
 * yet how long the instruction translation will be
 */
	return 0x19;
}

static int generate_icall(char *dest, instr_t *instr, trans_t *trans)
{
	int len = generate_call_head(dest, instr, trans);
	generate_ijump(&dest[len], instr, trans);
	trans->len += len;
/* XXX FUGLY */
	imm_to(&dest[0x15], (long)dest+trans->len);
	return trans->len;
}

static const char *ret_code  =
	"\xA3????"         /* mov %eax, scratch_stack-4 */
	"\x58"             /* pop %eax                  */
	"\x89\x25????"     /* mov %esp, scratch_stack   */
	"\xBC????"         /* mov $scratch_stack-4 %esp */
	"\x9c"             /* pushf                     */
	"\x3B\x05????"     /* cmp o_addr, %eax          */
	"\x2E\x75\x09"     /* jne, predict hit          */
	"\x9d"             /* popf                      */
	"\x58"             /* pop %eax                  */
	"\x5C"             /* pop %esp                  */
	"\xFF\x25????"     /* jmp *j_addr               */
	"\xA3????"         /* mov %eax, o_addr          */
	"\x68????"         /* push j_addr               */
	"\xFF\x25????"     /* jmp *runtime_ijmp_addr    */
;

static int generate_ret(char *dest, char *addr, trans_t *trans)
{
	char **cache = alloc_jmp_cache(addr);

	memcpy(dest, ret_code, 0x34);
	imm_to(&dest[0x01], (long)&scratch_stack-4);
	imm_to(&dest[0x08], (long)&scratch_stack);
	imm_to(&dest[0x0D], (long)&scratch_stack-4);
	imm_to(&dest[0x14], (long)&cache[0]);
	imm_to(&dest[0x20], (long)&cache[1]);
	imm_to(&dest[0x25], (long)&cache[0]);
	imm_to(&dest[0x2A], (long)&cache[1]);
	imm_to(&dest[0x30], (long)&runtime_ijmp_addr);

	*trans = (trans_t){ .len=0x34 };

	return 0x34;
}


static const char *ret_cleanup_code  =
	"\xA3????"         /* mov %eax, scratch_stack-4 */
	"\x58"             /* pop %eax                  */
	"\x89\x25????"     /* mov %esp, scratch_stack   */
	"\xBC????"         /* mov $scratch_stack-4 %esp */
	"\x9c"             /* pushf                     */
	"\x81\x44\x24\x08" /* add ??, 8(%esp)           */
	"??\x00\x00"
	"\x3B\x05????"     /* cmp o_addr, %eax          */
	"\x2E\x75\x09"     /* jne, predict hit          */
	"\x9d"             /* popf                      */
	"\x58"             /* pop %eax                  */
	"\x5C"             /* pop %esp                  */
	"\xFF\x25????"     /* jmp *j_addr               */
	"\xA3????"         /* mov %eax, o_addr          */
	"\x68????"         /* push j_addr               */
	"\xFF\x25????"     /* jmp *runtime_ijmp_addr    */
;

static int generate_ret_cleanup(char *dest, char *addr, trans_t *trans)
{
	char **cache = alloc_jmp_cache(addr);

	memcpy(dest, ret_cleanup_code, 0x3C);
	imm_to(&dest[0x01], (long)&scratch_stack-4);
	imm_to(&dest[0x08], (long)&scratch_stack);
	imm_to(&dest[0x0D], (long)&scratch_stack-4);
	dest[0x16] = addr[1];
	dest[0x17] = addr[2];
	imm_to(&dest[0x1C], (long)&cache[0]);
	imm_to(&dest[0x28], (long)&cache[1]);
	imm_to(&dest[0x2D], (long)&cache[0]);
	imm_to(&dest[0x32], (long)&cache[1]);
	imm_to(&dest[0x38], (long)&runtime_ijmp_addr);

	*trans = (trans_t){ .len=0x3C };

	return 0x3C;
}

static const char *linux_sysenter_code  =
	"\xFF\x25????"     /* call *linux_sysenter_emu_addr */
;

static int generate_linux_sysenter(char *dest, trans_t *trans)
{
	memcpy(&dest[0x0], linux_sysenter_code, 0x6);
	imm_to(&dest[0x2], (long)&linux_sysenter_emu_addr);
	*trans = (trans_t){ .len=0x6 };
	return 0x6;
}

static const char *int80_code  =
	"\x89\x25????"     /* mov %esp, scratch_stack   */
	"\xBC????"         /* mov $scratch_stack %esp   */
	"\x50"             /* push %eax                 */
	"\x68????"         /* push post_addr            */
	"\xFF\x15????"     /* call *int80_emu_addr      */
;

static int generate_int80(char *dest, instr_t *instr, trans_t *trans)
{
	memcpy(&dest[0x00], int80_code, 0x17);

	imm_to(&dest[0x02], (long)&scratch_stack);
	imm_to(&dest[0x07], (long)&scratch_stack);
	imm_to(&dest[0x0D], (long)&instr->addr[instr->len]);
	imm_to(&dest[0x13], (long)&int80_emu_addr);

	*trans = (trans_t){ .len=0x17 };

	return 0x17;
}

static const char *stub_code  =
	"\x89\x25????"     /* mov %esp, scratch_stack   */
	"\xBC????"         /* mov $scratch_stack %esp   */
	"\x50"             /* push %eax                 */
	"\x9c"             /* pushf                     */
	"\xB8????"         /* mov $o_addr, %eax         */
	"\x68????"         /* push j_addr               */
	"\xFF\x25????"     /* jmp *runtime_ijmp_addr    */
;

int generate_stub(char *dest, char *jmp_addr, char *imm_addr)
{
	char **cache = alloc_stub_cache(imm_addr, jmp_addr, dest);

	memcpy(imm_addr, &cache[1], 4);
	memcpy(&dest[0x00], stub_code, 0x1D);

	imm_to(&dest[0x02], (long)&scratch_stack);
	imm_to(&dest[0x07], (long)&scratch_stack);
	imm_to(&dest[0x0E], (long)jmp_addr);
	imm_to(&dest[0x13], (long)&cache[1]);
	imm_to(&dest[0x19], (long)&runtime_ijmp_addr);

	return 0x1D;
}

static int generate_ill(char *dest, trans_t *trans)
{
	dest[0] = '\x0F';
	dest[1] = '\x0B';
	*trans = (trans_t){ .len=2 };
	return 2;
}

static int copy_instr(char *dest, instr_t *instr, trans_t *trans)
{
	memcpy(dest, instr->addr, instr->len);
	*trans = (trans_t){ .len = instr->len };
	return trans->len;
}

static int taint_instr(char *dest, instr_t *instr, trans_t *trans)
{
	int len = 0;
	return len+copy_instr(&dest[len], instr, trans);
}

static void translate_control(char *dest, instr_t *instr, trans_t *trans,
                              char *map, unsigned long map_len)
{
	char *pc = instr->addr+instr->len;
	long imm=0, imm_len, off;

	imm_len=instr->len-instr->imm;
	if (instr->action == JF)
		imm_len -= 2;

	imm = imm_at(&instr->addr[instr->imm], imm_len);

	switch (instr->action)
	{
		case JC: /* conditional jump */
			generate_jcc(dest, pc+imm, instr->addr[instr->mrm-1]&0x0f,
			             trans, map, map_len);
			break;
		case JR: /* jump relative */
			generate_jump(dest, pc+imm, trans, map, map_len);
			break;
		case JF: /* far jump */
			generate_jump(dest, (char*)imm, trans, map, map_len);
			break;
		case JI: /* indirect jump */
			generate_ijump(dest, instr, trans);
			break;
		case CI: /* indirect call */
			generate_icall(dest, instr, trans);
			break;
		case R: /* return from call */
			generate_ret(dest, instr->addr, trans);
			break;
		case RC: /* return from call and pop some things */
			generate_ret_cleanup(dest, instr->addr, trans);
			break;
		case L: /* loops */
			if (instr->p[P4-PREFIX] == 0x67)
			{
				off = 1;
				dest[0] = '\x67';
			}
			else
				off = 0;

			dest[0+off] = instr->addr[instr->imm-1] ^ 1;
			dest[1+off] = '\x05';
			generate_jump(&dest[2+off], pc+imm, trans, map, map_len);
			trans->imm += 2+off; trans->len += 2+off;
			break;
		case CR:
			off = generate_call_head(dest, instr, trans);
			generate_jump(&dest[off], pc+imm, trans, map, map_len);
			trans->imm += off; trans->len += off;
			/* XXX FUGLY */
			imm_to(&dest[0x15], (long)dest+trans->len);
			break;
		case JOIN:
			generate_ill(dest, trans);
			break;
		default:
			die("unimplemented action: %d", instr->action);
	}

	if ( imm_len == 2 )
		trans->jmp_addr = (char *)((long)trans->jmp_addr & 0xffffL);
}

void translate_op(char *dest, instr_t *instr, trans_t *trans,
                  char *map, unsigned long map_len)
{
	if ( (instr->action & CONTROL_MASK) == CONTROL )
		translate_control(dest, instr, trans, map, map_len);
	else if ( (instr->action & TAINT_MASK) == TAINT )
		taint_instr(dest, instr, trans);
	else if (instr->action == C)
		copy_instr(dest, instr, trans);
	else if ( (instr->action == U) || (instr->action == JOIN) )
		generate_ill(dest, trans);
	else if (instr->action == INT)
	{
		if (instr->addr[1] == '\x80')
			generate_int80(dest, instr, trans);
		else
			copy_instr(dest, instr, trans);
	}
	else if (instr->action == SE)
		generate_linux_sysenter(dest, trans);
	else
			die("unimplemented action: %d", instr->action);
}

