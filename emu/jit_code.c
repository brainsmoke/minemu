
#include <string.h>

#include "jit_code.h"
#include "jmpcache.h"
#include "scratch.h"
#include "runtime.h"
#include "error.h"
#include "taint.h"
#include "debug.h"

/* op action */

#define C  COPY_INSTRUCTION
#define U  UNDEFINED_INSTRUCTION
#define BAD UNDEFINED_INSTRUCTION /* using these indicates an emulator bug */

#define JR JUMP_RELATIVE
#define JC JUMP_CONDITIONAL
#define JF JUMP_FAR
#define JI JUMP_INDIRECT

#define L  LOOP

#define CMOV CONDITIONAL_MOVE

#define CR CALL_RELATIVE
#define CF CALL_FAR
#define CI CALL_INDIRECT

#define R  RETURN
#define RC RETURN_CLEANUP
#define RF RETURN_FAR

#define SE  SYSENTER

#define XXX (C) /* todo */
#define PRIV (C)

#define TOMR ( TAINT | TAINT_OR      | TAINT_MODRM_TO_REG  )
#define TORM ( TAINT | TAINT_OR      | TAINT_REG_TO_MODRM  )
#define TXMR ( TAINT | TAINT_XOR     | TAINT_MODRM_TO_REG  )
#define TXRM ( TAINT | TAINT_XOR     | TAINT_REG_TO_MODRM  )
#define TCMR ( TAINT | TAINT_COPY    | TAINT_MODRM_TO_REG  )
#define TCRM ( TAINT | TAINT_COPY    | TAINT_REG_TO_MODRM  )
#define TCRP ( TAINT | TAINT_COPY    | TAINT_REG_TO_PUSH   )
#define TCMP ( TAINT | TAINT_COPY    | TAINT_MODRM_TO_PUSH )
#define TCPR ( TAINT | TAINT_COPY    | TAINT_POP_TO_REG    )
#define TCPM ( TAINT | TAINT_COPY    | TAINT_POP_TO_MODRM  )
#define TCAO ( TAINT | TAINT_COPY    | TAINT_AX_TO_OFFSET  )
#define TCOA ( TAINT | TAINT_COPY    | TAINT_OFFSET_TO_AX  )
#define TZMR ( TAINT | TAINT_COPY_ZX | TAINT_MODRM_TO_REG  )
#define TCSS ( TAINT | TAINT_COPY    | TAINT_STR_TO_STR    )
#define TCAS ( TAINT | TAINT_COPY    | TAINT_AX_TO_STR     )
#define TCSA ( TAINT | TAINT_COPY    | TAINT_STR_TO_AX     )
#define TSRM ( TAINT | TAINT_SWAP    | TAINT_REG_TO_MODRM  )
#define TSAR ( TAINT | TAINT_SWAP    | TAINT_AX_TO_REG     )
#define TPUA ( TAINT | TAINT_PUSHA                         )
#define TPPA ( TAINT | TAINT_POPA                          )
#define TLEA ( TAINT | TAINT_LEA                           )
#define TLVE ( TAINT | TAINT_LEAVE                         )
#define TER  ( TAINT | TAINT_ERASE   | TAINT_REG           )
#define TEM  ( TAINT | TAINT_ERASE   | TAINT_MODRM         )
#define TEP  ( TAINT | TAINT_ERASE   | TAINT_PUSH          )
#define TEH  ( TAINT | TAINT_ERASE   | TAINT_HIGH_REG      )
#define TED  ( TAINT | TAINT_ERASE   | TAINT_DX            )
#define TEA  ( TAINT | TAINT_ERASE   | TAINT_AX            )
#define TEAD ( TAINT | TAINT_ERASE   | TAINT_AX_DX         )

#define BORM ( TORM | TAINT_BYTE )
#define BOMR ( TOMR | TAINT_BYTE )
#define BXRM ( TXRM | TAINT_BYTE )
#define BXMR ( TXMR | TAINT_BYTE )
#define BCRM ( TCRM | TAINT_BYTE )
#define BCMR ( TCMR | TAINT_BYTE )
#define BCAO ( TCAO | TAINT_BYTE )
#define BCOA ( TCOA | TAINT_BYTE )
#define BCSS ( TCSS | TAINT_BYTE )
#define BCAS ( TCAS | TAINT_BYTE )
#define BCSA ( TCSA | TAINT_BYTE )
#define BSRM ( TSRM | TAINT_BYTE )
#define BEA  ( TEA  | TAINT_BYTE )
#define BER  ( TER  | TAINT_BYTE )
#define BEP  ( TEP  | TAINT_BYTE )
#define BEM  ( TEM  | TAINT_BYTE )
#define BEAD ( TEAD | TAINT_BYTE )
#define BZMR ( TZMR | TAINT_BYTE )

const unsigned char jit_action[] =
{
	[MAIN_OPTABLE] =
/*        ?0   ?1   ?2   ?3   ?4   ?5   ?6   ?7   ?8   ?9   ?A   ?B   ?C   ?D   ?E   ?F  */
/* 0? */ BORM,TORM,BOMR,TOMR,  C ,  C ,  C ,  C ,BORM,TORM,BOMR,TOMR,  C ,  C ,  C , BAD,
/* 1? */ BORM,TORM,BOMR,TOMR,  C ,  C ,  C ,  C ,BXRM,TXRM,BXMR,TXMR,  C ,  C ,  C ,  C ,
/* 2? */ BORM,TORM,BOMR,TOMR,  C ,  C , BAD,  C ,BXRM,TXRM,BXMR,TXMR,  C ,  C , BAD,  C ,
/* 3? */ BXRM,TXRM,BXMR,TXMR,  C ,  C , BAD,  C ,  C ,  C ,  C ,  C ,  C ,  C , BAD,  C ,
/* 4? */   C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,
/* 5? */ TCRP,TCRP,TCRP,TCRP,TCRP,TCRP,TCRP,TCRP,TCPR,TCPR,TCPR,TCPR,TCPR,TCPR,TCPR,TCPR,
/* 6? */ TPUA,TPPA,  C ,PRIV, BAD, BAD, BAD, BAD, TEP,TCMR, TEP,TCMR,PRIV,PRIV,PRIV,PRIV,
/* 7? */  JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC ,
/* 8? */   C ,  C ,  C ,  C ,  C ,  C ,BSRM,TSRM,BCRM,TCRM,BCMR,TCMR, XXX,TLEA, XXX,TCPM,
/* 9? */   C ,TSAR,TSAR,TSAR,TSAR,TSAR,TSAR,TSAR, TEH, TED,  CF,  C , TEP,  C ,  C , BEA,
/* A? */ BCOA,TCOA,BCAO,TCAO,BCSS,TCSS,  C ,  C ,  C ,  C ,BCAS,TCAS,BCSA,TCSA,  C ,  C ,
/* B? */  BER, BER, BER, BER, BER, BER, BER, BER, TER, TER, TER, TER, TER, TER, TER, TER,
/* C? */  XXX, XXX,  RC,  R , XXX, XXX, BEM, TEM, XXX,TLVE,  RF,  RF,  C , INT,  C ,  C ,
/* D? */  XXX, XXX, XXX, XXX,  C ,  C ,  C , XXX,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,
/* E? */   L ,  L ,  L ,  L ,PRIV,PRIV,PRIV,PRIV, CR,  JR,  JF , JR ,PRIV,PRIV,PRIV,PRIV,
/* F? */  BAD,  U , BAD, BAD,PRIV,  C , BAD, BAD,  C ,  C ,  C ,  C ,  C ,  C ,  C , BAD,

	[ESC_OPTABLE] =
/*        ?0   ?1   ?2   ?3   ?4   ?5   ?6   ?7   ?8   ?9   ?A   ?B   ?C   ?D   ?E   ?F */
/* 0? */ PRIV,PRIV,PRIV,PRIV,  C ,  U ,  C ,  U ,  C ,  C ,  C ,  U ,  C ,  C ,  C ,  C ,
/* 1? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,
/* 2? */ PRIV,PRIV,PRIV,PRIV,  C ,  C ,  C ,  C , XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,
/* 3? */ PRIV,BEAD,BEAD,BEAD, SE ,  C ,  C ,PRIV, BAD,  C , BAD,  C ,  C ,  C ,  C ,  C ,
/* 4? */ CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,CMOV,
/* 5? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,
/* 6? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,
/* 7? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX,  C ,PRIV,PRIV,  C ,  C , XXX, XXX, XXX, XXX,
/* 8? */  JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC , JC ,
/* 9? */  BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM, BEM,
/* A? */  BEP, BER, XXX,  C , XXX, XXX,  C ,  C , BEP, BEM,PRIV,  C , XXX, XXX, XXX,TOMR,
/* B? */  XXX, XXX,  C ,  C ,  C ,  C ,BZMR,TZMR, XXX,  C ,  C ,  C ,  C ,  C ,BZMR,TZMR,
/* C? */ BORM,TORM,  C ,TCRM,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,  C ,
/* D? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,
/* E? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,
/* F? */  XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX, XXX,

	[G38_OPTABLE] =
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

	[G3A_OPTABLE] =
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

	[GF6_OPTABLE] =
          C , U , C , C ,XXX,XXX,XXX,XXX,

	[GF7_OPTABLE] =
          C , U , C , C ,XXX,XXX,XXX,XXX,

	[GFF_OPTABLE] =
          C , C , CI, U , JI, U ,TCMP, U ,

	[BAD_OP] = U,

	[CUTOFF_OP] = JOIN,
};

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

int gen_code(char *dst, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int i,j=0, c, outc=1;
#ifdef EMU_DEBUG
	dst[0] = '\xC7'; dst[1] = '\x05';
	imm_to(&dst[2], (long)&last_jit);
	imm_to(&dst[6], (long)&dst[20]);
	j=10;
#endif
	for (i=0; (c=fmt[i]); i++)
	{
		if ( (unsigned int)(c-'0') < 10 )
			outc = outc*16 + (c-'0');
		else if ( (unsigned int)(c-'A') < 6 )
			outc = outc*16 + (c-'A'+10);
		else if ( (unsigned int)(c-'a') < 6 )
			outc = outc*16 + (c-'a'+10);
		else switch (c)
		{
			case 'L': case 'l':
			{
				long l = va_arg(ap, long);
				imm_to(&dst[j], l);
				j += 4;
				break;
			}
			case 'S': case 's':
			{
				short s = va_arg(ap, int);
				dst[j]   = (char)s;
				dst[j+1] = (char)(s>>8);
				j += 2;
				break;
			}
			case '&':
			{
				int *ix = va_arg(ap, int *);
				*ix = j;
				break;
			}
			case '$':
			{
				char *s = va_arg(ap, char *);
				int len = va_arg(ap, int);
				memcpy(&dst[j], s, len);
				j+=len;
				break;
			}
			case '+':
			{
				char x = va_arg(ap, int);
				dst[j-1] += x;
				break;
			}
			case '.':
			{
				char x = va_arg(ap, int);
				dst[j] = x;
				j++;
				break;
			}
			case '?':
			{
				char x = va_arg(ap, int);
				if (x)
				{
					dst[j] = x;
					j++;
				}
				break;
			}
			default:
				break;
		}
		if (outc > 0xff)
		{
			dst[j] = (char)outc;
			outc = 1;
			j++;
		}
	}
	va_end(ap);
	return j;
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

static int generate_ijump_tail(char *dest, char **jmp_orig, char **jmp_jit)
{
	return gen_code(
		dest,

		"89 25 L"     /* mov %esp, scratch_stack   */
		"BC    L"     /* mov $scratch_stack-4 %esp */
		"9C"          /* pushf                     */
#ifdef EMU_DEBUG
"FF 04 25 L"          /* incl ijmp_count           */
#endif
		"3B 05 L"     /* cmp o_addr, %eax          */
		"2E 75 09"    /* jne, predict hit          */
		"9D"          /* popf                      */
		"58"          /* pop %eax                  */
		"5C"          /* pop %esp                  */
		"FF 25 L"     /* jmp *j_addr               */
#ifdef EMU_DEBUG
"FF 04 25 L"          /* incl ijmp_misses          */
#endif
		"A3    L"     /* mov %eax, o_addr          */
		"68    L"     /* push j_addr               */
		"FF 25 L",    /* jmp *runtime_ijmp_addr    */

		scratch_stack,
		&scratch_stack[-1],
#ifdef EMU_DEBUG
&ijmp_count,
#endif
		jmp_orig,
		jmp_jit,
#ifdef EMU_DEBUG
&ijmp_misses,
#endif
		jmp_orig,
		jmp_jit,
		&runtime_ijmp_addr
	);
}

static int generate_ijump(char *dest, instr_t *instr, trans_t *trans)
{
	char **cache = alloc_jmp_cache(instr->addr);
	long mrm_len = instr->len - instr->mrm;
	int i;

	int len = gen_code(
		dest,
		"A3 L"           /* mov %eax, scratch_stack-4 */
		"? 8B &$",       /* mov ... ( -> %eax )       */
		&scratch_stack[-1],
		instr->p[2], &i, &instr->addr[instr->mrm], mrm_len
	);

	dest[i] &= 0xC7; /* -> %eax */
	len += generate_ijump_tail(&dest[len], &cache[0], &cache[1]);
	*trans = (trans_t){ .len = len };

	return len;
}

static int generate_call_head(char *dest, instr_t *instr, trans_t *trans, int *retaddr_index)
{
	int hash = HASH_INDEX(&instr->addr[instr->len]);
	/* XXX FUGLY as a speed optimisation, we insert the return address
	 * directly into the cache, this makes relocating code more messy.
	 * proposed fix: scan memory for call instructions upon relocation,
	 * change the address in-place
	 */
	/* XXX FUGLY translated address is inserted by caller since we don't know
	 * yet how long the instruction translation will be
	 */
	return gen_code(
		dest,

		"68 L"                /* push $retaddr */
		"C7 05 L L"           /* movl $addr, jmp_list.addr[HASH_INDEX(addr)] */
		"C7 05 L & DEADBEEF", /* movl $jit_addr, jmp_list.jit_addr[HASH_INDEX(addr)] */

		&instr->addr[instr->len],
		&jmp_list.addr[hash],       &instr->addr[instr->len],
		&jmp_list.jit_addr[hash],   retaddr_index
	);
}

static int generate_icall(char *dest, instr_t *instr, trans_t *trans)
{
	int hash = HASH_INDEX(&instr->addr[instr->len]);
	char **cache = alloc_jmp_cache(instr->addr);
	long mrm_len = instr->len - instr->mrm;
	int mrm, retaddr_index;

	/* XXX FUGLY as a speed optimisation, we insert the return address
	 * directly into the cache, this makes relocating code more messy.
	 * proposed fix: scan memory for call instructions upon relocation,
	 * change the address in-place
	 */
	int len = gen_code(
		dest,

		"A3 L"                /* mov %eax, scratch_stack-4 */
		"? 8B &$"             /* mov ... ( -> %eax )       */
		"68 L"                /* push $retaddr */
		"C7 05 L L"           /* movl $addr, jmp_list.addr[HASH_INDEX(addr)] */
		"C7 05 L & DEADBEEF", /* movl $jit_addr, jmp_list.jit_addr[HASH_INDEX(addr)] */

		&scratch_stack[-1],
		instr->p[2], &mrm, &instr->addr[instr->mrm], mrm_len,
		&instr->addr[instr->len],
		&jmp_list.addr[hash],       &instr->addr[instr->len],
		&jmp_list.jit_addr[hash],   &retaddr_index
	);

	dest[mrm] &= 0xC7; /* -> %eax */
	len += generate_ijump_tail(&dest[len], &cache[0], &cache[1]);
	imm_to(&dest[retaddr_index], ((long)dest)+len);
	*trans = (trans_t){ .len = len };
	return len;
}

static int generate_ret(char *dest, char *addr, trans_t *trans)
{
	int len = gen_code(
		dest,

		"A3 L"           /* mov %eax, scratch_stack-4 */
		"58"             /* pop %eax                  */
		"89 25 L"        /* mov %esp, scratch_stack   */
		"BC L"           /* mov $scratch_stack-4 %esp */
		"9C"             /* pushf                     */
		"51"             /* push %ecx                 */
		"0F B7 C8"       /* movzx %ax, %ecx           */
#ifdef EMU_DEBUG
"FF 04 25 L"             /* incl ret_count            */
#endif
		"3B 04 8D L"     /* cmp &jmp_list.addr[0](,%ecx,4), %eax     */
		"2E 75 16"       /* jne, predict hit          */
		"8B 04 8D L"     /* mov &jmp_list.jit_addr[0](,%ecx,4), %eax */
		"59"             /* pop %ecx                  */
		"9D"             /* popf                      */
		"A3 L"           /* mov %eax, jit_eip         */
		"58"             /* pop %eax                  */
		"5C"             /* pop %esp                  */
		"FF 25 L"        /* jmp *j_addr               */
		"FF 25 L",       /* jmp *runtime_ijmp_addr    */

		&scratch_stack[-1],
		scratch_stack,
		&scratch_stack[-1],
#ifdef EMU_DEBUG
&ret_count,
#endif
		&jmp_list.addr[0],
		&jmp_list.jit_addr[0],
		&jit_eip,
		&jit_eip,
		&runtime_ret_addr
	);

	*trans = (trans_t){ .len=len };

	return len;
}

static int generate_ret_cleanup(char *dest, char *addr, trans_t *trans)
{
	int len = gen_code(
		dest,

		"A3 L"           /* mov %eax, scratch_stack-4 */
		"58"             /* pop %eax                  */
		"89 25 L"        /* mov %esp, scratch_stack   */
		"BC L"           /* mov $scratch_stack-4 %esp */
		"9C"             /* pushf                     */
		"81 44 24 08"    /* add ??, 8(%esp)           */
		"S 00 00"
		"51"             /* push %ecx                 */
		"0F B7 C8"       /* movzx %ax, %ecx           */
#ifdef EMU_DEBUG
"FF 04 25 L"             /* incl ret_count            */
#endif
		"3B 04 8D L"     /* cmp &jmp_list.addr[0](,%ecx,4), %eax     */
		"2E 75 16"       /* jne, predict hit          */
		"8B 04 8D L"     /* mov &jmp_list.jit_addr[0](,%ecx,4), %eax */
		"59"             /* pop %ecx                  */
		"9D"             /* popf                      */
		"A3 L"           /* mov %eax, jit_eip         */
		"58"             /* pop %eax                  */
		"5C"             /* pop %esp                  */
		"FF 25 L"        /* jmp *j_addr               */
		"FF 25 L",       /* jmp *runtime_ijmp_addr    */

		&scratch_stack[-1],
		scratch_stack,
		&scratch_stack[-1],
		addr[1] + (addr[2]<<8),
#ifdef EMU_DEBUG
&ret_count,
#endif
		&jmp_list.addr[0],
		&jmp_list.jit_addr[0],
		&jit_eip,
		&jit_eip,
		&runtime_ret_addr
	);

	*trans = (trans_t){ .len=len };

	return len;
}

static int generate_linux_sysenter(char *dest, trans_t *trans)
{
	int len = gen_code(
		dest,
		"FF 25 L",     /* jmp *linux_sysenter_emu_addr */
		&linux_sysenter_emu_addr
	);
	*trans = (trans_t){ .len=len };
	return len;
}

static int generate_int80(char *dest, instr_t *instr, trans_t *trans)
{
	int len = gen_code(
		dest,
		"C7 05 L L"      /* movl $post_addr, user_eip */
		"FF 25 L",       /* jmp *int80_emu_addr       */

		&user_eip, &instr->addr[instr->len],
		&int80_emu_addr);

	*trans = (trans_t){ .len=len };

	return len;
}

int generate_stub(char *dest, char *jmp_addr, char *imm_addr)
{
	char **cache = alloc_stub_cache(imm_addr, jmp_addr, dest);
	memcpy(imm_addr, &cache[1], 4);
	
	int len = gen_code(
		dest,

		"89 25 L"        /* mov %esp, scratch_stack   */
		"BC L"           /* mov $scratch_stack %esp   */
		"50"             /* push %eax                 */
		"9C"             /* pushf                     */
		"B8 L"           /* mov $o_addr, %eax         */
		"68 L"           /* push j_addr               */
		"FF 25 L",       /* jmp *runtime_ijmp_addr    */

		scratch_stack,
		scratch_stack,
		jmp_addr,
		&cache[1],
		&runtime_ijmp_addr);

	return len;
}

int generate_ill(char *dest, trans_t *trans)
{
	dest[0] = '\x0F';
	dest[1] = '\x0B';
	*trans = (trans_t){ .len=2 };
	return 2;
}

static int generate_cmov(char *dest, instr_t *instr, trans_t *trans)
{
	int mrm = instr->mrm, len = instr->len;
	char *addr = instr->addr;
	dest[0] = '\x70' + ( (addr[mrm-1]&0xf) ^ 1 );
	dest[1] = len-1;
	memcpy(&dest[2], addr, mrm-2);
	dest[mrm] = '\x8b';
	memcpy(&dest[mrm+1], &addr[mrm], len-mrm);
	*trans = (trans_t){ .len = len+1 };
	return trans->len;
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
	int retaddr_index;

	imm_len=instr->len-instr->imm;
	if (jit_action[instr->op] == JUMP_FAR)
		imm_len -= 2;

	imm = imm_at(&instr->addr[instr->imm], imm_len);

	switch (jit_action[instr->op])
	{
		case JUMP_CONDITIONAL:
			generate_jcc(dest, pc+imm, instr->addr[instr->mrm-1]&0x0f,
			             trans, map, map_len);
			break;
		case JUMP_RELATIVE:
			generate_jump(dest, pc+imm, trans, map, map_len);
			break;
		case JUMP_FAR:
			generate_jump(dest, (char*)imm, trans, map, map_len);
			break;
		case JUMP_INDIRECT:
			generate_ijump(dest, instr, trans);
			break;
		case CALL_INDIRECT:
			generate_icall(dest, instr, trans);
			break;
		case RETURN:
			generate_ret(dest, instr->addr, trans);
			break;
		case RETURN_CLEANUP: /* return from call and pop some things */
			generate_ret_cleanup(dest, instr->addr, trans);
			break;
		case LOOP: /* loops */
			off = gen_code(
				dest,

				"$  02"
				"EB 05",

				instr->addr, instr->len-1
			);

			generate_jump(&dest[off], pc+imm, trans, map, map_len);
			if (trans->imm)
				trans->imm += off;
			trans->len += off;
			break;
		case CALL_RELATIVE:
			off = generate_call_head(dest, instr, trans, &retaddr_index);
			generate_jump(&dest[off], pc+imm, trans, map, map_len);
			if (trans->imm)
				trans->imm += off;
			trans->len += off;
			/* XXX FUGLY */
			imm_to(&dest[retaddr_index], (long)dest+trans->len);
			break;
		case JOIN:
			generate_ill(dest, trans);
			break;
		default:
			die("unimplemented action: %d", jit_action[instr->op]);
	}

	if ( imm_len == 2 )
		trans->jmp_addr = (char *)((long)trans->jmp_addr & 0xffffL);
}

void translate_op(char *dest, instr_t *instr, trans_t *trans,
                  char *map, unsigned long map_len)
{
	int action = jit_action[instr->op];

	if ( (action & CONTROL_MASK) == CONTROL )
		translate_control(dest, instr, trans, map, map_len);
	else if ( (action & TAINT_MASK) == TAINT )
		taint_instr(dest, instr, trans);
	else if (action == COPY_INSTRUCTION)
		copy_instr(dest, instr, trans);
	else if (action == CONDITIONAL_MOVE)
		generate_cmov(dest, instr, trans);
	else if ( (action == UNDEFINED_INSTRUCTION) || (action == JOIN) )
		generate_ill(dest, trans);
	else if (action == INT)
	{
		if (instr->addr[1] == '\x80')
			generate_int80(dest, instr, trans);
		else
			copy_instr(dest, instr, trans);
	}
	else if (action == SYSENTER)
		generate_linux_sysenter(dest, trans);
	else
			die("unimplemented action: %d", action);
}

