
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>

#include "debug.h"
#include "lib.h"
#include "scratch.h"
#include "error.h"
#include "sigwrap.h"

static int out = 2;

const char *sigframe_pre_desc[] =
{
	"    [*pretcode] [   sig   ]",
};

const char *sigframe_post_desc[] =
{
	"    [extramask] [         retcode        ]",
};

const char *rt_sigframe_pre_desc[] =
{
	"    [*pretcode] [   sig   ]    [ *pinfo  ] [   *puc   ]",
};

const char *rt_sigframe_post_desc[] =
{
	"    [       retcode       ]",
};

const char *ucontext_pre_desc[] =
{
	"    [ uc_flags] [ *uc_link]    [  *ss_sp ] [ ss_flags ]",
	"    [ ss_size ]"
};

const char *ucontext_post_desc[] =
{
	"    [      uc_sigmask     ]"
};

const char *sigcontext_desc[] =
{
	"    [ gs] [gsh] [ fs] [fsh]    [ es] [esh] [ ds] [dsh]",
	"    [   edi   ] [   esi   ]    [   ebp   ] [   esp   ]",
	"    [   ebx   ] [   edx   ]    [   ecx   ] [   eax   ]",
	"    [  trapno ] [   err   ]    [   eip   ] [ cs] [csh]",
	"    [  eflags ] [ esp@sig ]    [ ss] [ssh] [ *fpstate]",
	"    [  oldmask] [   cr2   ]",
};

const char *fpstate_desc[] =
{
	"    [    cw   ] [    sw   ]    [   tag   ] [  ipoff  ]",
	"    [  cssel  ] [ dataoff ]    [ datasel ] [  st[0]   ",
	"                    ] [            st[1]             ]",
	"    [            st[2]             ] [         st[3]  ",
	"              ] [             st[4]            ] [    ",
	"           st[5]          ]    [             st[6]    ",
    "        ] [            st[7]             ] [sts] [mgc]",
    "    [         ] [         ]    [         ] [         ]",
    "    [         ] [         ]    [  mxcsr  ] [ reserved]",
    "    [      _fxsr_st[0]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[1]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[2]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[3]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[4]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[5]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[6]         <   exp  >] [ padding ]",
    "    [      _fxsr_st[7]         <   exp  >] [ padding ]",
    "    [                    _xmm[0]                     ]",
    "    [                    _xmm[1]                     ]",
    "    [                    _xmm[2]                     ]",
    "    [                    _xmm[3]                     ]",
    "    [                    _xmm[4]                     ]",
    "    [                    _xmm[5]                     ]",
    "    [                    _xmm[6]                     ]",
    "    [                    _xmm[7]                     ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",
    "    [                                                ]",

};
/*

static FILE *out = NULL;

void debug_init(FILE *outfile)
{
	out = outfile;
}

FILE *debug_out(void)
{
	return out;
}

#ifdef __i386__

const char *registers_desc[] =
{
	"    [   ebx   ] [   ecx   ]    [   edx   ] [   esi   ]",
	"    [   edi   ] [   ebp   ]    [   eax   ] [ ds] [_ds]",
	"    [ es] [_es] [ fs] [_fs]    [ gs] [_gs] [ orig eax]",
	"    [   eip   ] [ cs] [_cs]    [  eflags ] [   esp   ]",
	"    [ ss] [_ss]",
};

#endif

#ifdef __x86_64__
#error structure has changed, needs to be retested

const char *registers_desc[] =
{
	"    [         r15         ]    [         r14         ]",
	"    [         r13         ]    [         r12         ]",
	"    [         rbp         ]    [         rbx         ]",
	"    [         r11         ]    [         r10         ]",
	"    [         r9          ]    [         r8          ]",
	"    [         rax         ]    [         rcx         ]",
	"    [         rsi         ]    [         rdi         ]",
	"    [       orig_rax      ]    [         rip         ]",
	"    [          cs         ]    [        eflags       ]",
	"    [         rsp         ]    [          ss         ]",
	"    [       fs_base       ]    [        gs_base      ]",
	"    [          ds         ]    [          es         ]",
	"    [          fs         ]    [          gs         ]",
};

#endif
*/

static inline long min(long a, long b) { return a<b ? a:b; }
static inline long max(long a, long b) { return a>b ? a:b; }

static const char *hi = "\033[0;34m", *reset = "\033[m";

/* Prints up to 16 characters in hexdump style with optional colors
 * if `ascii' is non-zero, an additional ascii representation is printed
 */
static void printhex_line(const void *data, ssize_t len, int ascii,
                          const int indices[], const char *colors[])
{
	int i, cur = -1;
	char c;

	for (i=0; i<16; i++)
	{
		if (i % 8 == 0)
			fd_printf(out, "   ");

		if (i < len)
		{
			if ( indices && colors && (cur != indices[i]) )
				fd_printf(out, "%s", colors[cur = indices[i]]);

			fd_printf(out, " %02x", ((unsigned char *)data)[i]);
		}
		else
			fd_printf(out, "   ");
	}

	if (indices && colors)
		fd_printf(out, "\033[m");

	if (ascii && len > 0)
	{
		cur = -1;
		fd_printf(out, "    |");

		for (i=0; i<16; i++)
		{
			if (i == len)
				break;

			if ( indices && colors && (cur != indices[i]) )
				fd_printf(out, "%s", colors[cur = indices[i]]);

			c = ((unsigned char *)data)[i];
			fd_printf(out, "%c", isprint(c)?c:'.');
		}

		if (indices && colors)
			fd_printf(out, "\033[m");

		fd_printf(out, "|");
	}

	fd_printf(out, "\n");
}

static void printhex_descr(const void *data, ssize_t len, int ascii,
                           const char *descriptions[])
{
	ssize_t row;

	for (row=0; row*16<len; row++)
	{
		if ( descriptions != NULL )
			fd_printf(out, "%s%s%s\n", hi, descriptions[row], reset);

		printhex_line((char*)data+row*16, min(16, len-row*16),
		              ascii, NULL, NULL);
	}
}

void printhex(const void *data, int len)
{
	printhex_descr(data, len, 1, NULL);
}

static void printhex_diff_descr(const void *data1, ssize_t len1,
                                const void *data2, ssize_t len2,
                                int grane, int ascii,
                                const char *descriptions[])
{
	ssize_t row, i;

	enum { NODIFF=0, DIFF=1 };
	int d[16], diff = 0;

	const char *color1[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[0;37m" };
	const char *color2[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[1;30m" };
	ssize_t minlen = min(len1, len2);
	ssize_t maxlen = max(len1, len2);

	for (row=0; row*16<maxlen; row++)
	{
		if ( descriptions != NULL )
			fd_printf(out, "%s%s%s\n", hi, descriptions[row], reset);

		for (i=0; i<16; i++)
		{
			if ( (row*16+i) % grane == 0 )
			{
				if ( (minlen != maxlen && minlen-i-row*16 < grane) ||
				      memcmp( (char*)data1+row*16+i,
				              (char*)data2+row*16+i,
				              min(grane, minlen-i-row*16)) )
					diff = DIFF;
				else
					diff = NODIFF;
			}

			d[i] = diff;
		}

		printhex_line((char*)data1+row*16, min(16, len1-row*16),
		              ascii, d, color1);
		printhex_line((char*)data2+row*16, min(16, len2-row*16),
		              ascii, d, color2);
	}
}

void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane)
{
	printhex_diff_descr(data1, len1, data2, len2, grane, 1, NULL);
}

#ifdef EMU_DEBUG
void print_debug_data(void)
{
	debug("counts/misses ijmp: %u/%u ret: %u/%u", ijmp_count, ijmp_misses, ret_count, ret_misses);
}
#endif

void print_sigcontext(struct sigcontext *sc)
{
	printhex_descr(sc, sizeof(*sc), 1, sigcontext_desc);
}

void print_sigcontext_diff(struct sigcontext *sc1, struct sigcontext *sc2)
{
	printhex_diff_descr(sc1, sizeof(*sc1), sc2, sizeof(*sc2), sizeof(long), 1, sigcontext_desc);
}

void print_fpstate(struct _fpstate *fpstate)
{
	printhex_descr(fpstate, sizeof(*fpstate), 1, fpstate_desc);
}

void print_fpstate_diff(struct _fpstate *fpstate1, struct _fpstate *fpstate2)
{
	printhex_diff_descr(fpstate1, sizeof(*fpstate1), fpstate2, sizeof(*fpstate2), sizeof(long), 1, fpstate_desc);
}

void print_siginfo(siginfo_t *info)
{
	printhex(info, sizeof(*info));
}

void print_siginfo_diff(siginfo_t *info1, siginfo_t *info2)
{
	printhex_diff(info1, sizeof(*info1), info2, sizeof(*info2), sizeof(long));
}

void print_ucontext(struct kernel_ucontext *uc)
{
	printhex_descr(uc, offsetof(struct kernel_ucontext, uc_mcontext), 1, ucontext_pre_desc);
	print_sigcontext(&uc->uc_mcontext);
	printhex_descr(&uc->uc_sigmask, sizeof(uc->uc_sigmask), 1, ucontext_post_desc);
}

void print_ucontext_diff(struct kernel_ucontext *uc1, struct kernel_ucontext *uc2)
{
	printhex_diff_descr(uc1, offsetof(struct kernel_ucontext, uc_mcontext),
	                    uc2, offsetof(struct kernel_ucontext, uc_mcontext), sizeof(long), 1, ucontext_pre_desc);
	print_sigcontext_diff(&uc1->uc_mcontext, &uc2->uc_mcontext);
	printhex_diff_descr(&uc1->uc_sigmask, sizeof(uc1->uc_sigmask),
	                    &uc2->uc_sigmask, sizeof(uc2->uc_sigmask), sizeof(long), 1, ucontext_post_desc);
}

void print_rt_sigframe(struct kernel_rt_sigframe *frame)
{
	debug("start: %x, end: %x, size: %x", frame, &frame[1], sizeof(*frame));
	debug("@ %x", frame);
	printhex_descr(frame, 16, 1, rt_sigframe_pre_desc);
	debug("@ %x", &frame->info);
	print_siginfo(&frame->info);
	debug("@ %x", &frame->uc);
	print_ucontext(&frame->uc);
	debug("@ %x", &frame->fpstate);
	print_fpstate(&frame->fpstate);
	debug("@ %x", &frame->retcode);
	printhex_descr(&frame->retcode, 8, 1, rt_sigframe_post_desc);
}

void print_rt_sigframe_diff(struct kernel_rt_sigframe *frame1, struct kernel_rt_sigframe *frame2)
{
	printhex_diff_descr(frame1, 16, frame2, 16, sizeof(long), 1, rt_sigframe_pre_desc);
	print_siginfo_diff(&frame1->info, &frame2->info);
	print_ucontext_diff(&frame1->uc, &frame2->uc);
	print_fpstate_diff(&frame1->fpstate, &frame2->fpstate);
	printhex_diff_descr(&frame1->retcode, 8, &frame2->retcode, 8, sizeof(long), 1, rt_sigframe_post_desc);
}

void print_sigframe(struct kernel_sigframe *frame)
{
	debug("start: %x, end: %x, size: %x", frame, &frame[1], sizeof(*frame));
	debug("@ %x", frame);
	printhex_descr(frame, 8, 1, sigframe_pre_desc);
	debug("@ %x", &frame->sc);
	print_sigcontext(&frame->sc);
	debug("@ %x", &frame->fpstate);
	print_fpstate(&frame->fpstate);
	debug("@ %x", &frame->extramask);
	printhex_descr(&frame->extramask, 12, 1, sigframe_post_desc);
}

void print_sigframe_diff(struct kernel_sigframe *frame1, struct kernel_sigframe *frame2)
{
	printhex_diff_descr(frame1, 8, frame2, 8, sizeof(long), 1, sigframe_pre_desc);
	print_sigcontext_diff(&frame1->sc, &frame2->sc);
	print_fpstate_diff(&frame1->fpstate, &frame2->fpstate);
	printhex_diff_descr(&frame1->extramask, 12, &frame2->extramask, 12, sizeof(long), 1, sigframe_post_desc);
}


