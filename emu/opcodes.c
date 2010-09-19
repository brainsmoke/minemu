
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

#define C  COPY_INSTRUCTION
#define U  UNDEFINED_INSTRUCTION

#define JR JUMP_RELATIVE
#define JC JUMP_CONDITIONAL
#define JF JUMP_FAR
#define JI JUMP_INDIRECT

#define L  LOOP

#define CR CALL_RELATIVE
#define CF CALL_FAR
#define CI CALL_INDIRECT

#define R  RETURN
#define RC RETURN_CLEANUP
#define RF RETURN_FAR

#define IA (CONTROL|11)

#define SE  SYSENTER

#define XXX (C) /* todo */
#define IGN (C) /* todo */

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
			case P1:
				if (instr->p1 != 0)
				{
					instr->action = U;
					return CODE_ERR;
				}
				instr->p1 = code;
				continue;
			case P2:
				if (instr->p2 != 0)
				{
					instr->action = U;
					return CODE_ERR;
				}
				instr->p2 = code;
				continue;
			case P3:
				if (instr->p3 != 0)
				{
					instr->action = U;
					return CODE_ERR;
				}
				instr->p3 = code;
				continue;
			case P4:
				if (instr->p4 != 0)
				{
					instr->action = U;
					return CODE_ERR;
				}
				instr->p4 = code;
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

			case BATSHIT:
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
			case 0x18: instr->action = U;  break; /* indirect far call */
			case 0x20: instr->action = JI; break; /* indirect jump */
			case 0x28: instr->action = U;  break; /* indirect far jump */
			default:
				instr->action = C;
		}

		if (instr->p4 == 0x67)
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
		if (instr->p3 == 0x66)
			instr->len += 2;
		else
			instr->len += 4;
	}

	if (type & IMMA)
	{
		if (instr->p4 == 0x67)
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
	return ret==CODE_JOIN ? -1 : instr.len;
}

