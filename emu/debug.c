#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <fcntl.h>

#include "debug.h"
#include "lib.h"
#include "mm.h"
#include "jit.h"
#include "scratch.h"
#include "error.h"
#include "sigwrap.h"
#include "syscalls.h"
#include "jit_fragment.h"

#include "hexdump.h"

static int out = 2;

const char *sigframe_pre_desc[] =
{
	"[*pretcode] [   sig   ]",
};

const char *sigframe_post_desc[] =
{
	"[extramask] [        retcode       ]",
};

const char *rt_sigframe_pre_desc[] =
{
	"[*pretcode] [   sig   ]  [ *pinfo  ] [   *puc   ]",
};

const char *rt_sigframe_post_desc[] =
{
	"[       retcode       ]",
};

const char *ucontext_pre_desc[] =
{
	"[ uc_flags] [ *uc_link]  [  *ss_sp ] [ ss_flags ]",
	"[ ss_size ]"
};

const char *ucontext_post_desc[] =
{
	"[      uc_sigmask     ]"
};

const char *sigcontext_desc[] =
{
	"[ gs] [gsh] [ fs] [fsh]  [ es] [esh] [ ds] [dsh]",
	"[   edi   ] [   esi   ]  [   ebp   ] [   esp   ]",
	"[   ebx   ] [   edx   ]  [   ecx   ] [   eax   ]",
	"[  trapno ] [   err   ]  [   eip   ] [ cs] [csh]",
	"[  eflags ] [ esp@sig ]  [ ss] [ssh] [ *fpstate]",
	"[  oldmask] [   cr2   ]",
};

const char *regs_desc[] =
{
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
};

const char *stat64_desc[] =
{
	"[        st_dev       ]  [xxxxxxxxxxx __st_ino ]",
	"[ st_mode ] [ st_nlink]  [  st_uid ] [  st_gid ]",
	"[        st_rdev      ]  [xxxxxxxxx] [  st_size ",
	"          ] [_blksize ]  [      st_blocks      ]",
	"[  atime  ] [ ..nsec  ]  [  mtime  ] [ ..nsec  ]",
	"[  ctime  ] [ ..nsec  ]  [         ino         ]",
};

const char *fpstate_desc[] =
{
	"[    cw   ] [    sw   ]  [   tag   ] [  ipoff  ]",
	"[  cssel  ] [ dataoff ]  [ datasel ] [  st[0]   ",
	"                ] [          st[1]             ]",
	"[            st[2]           ] [         st[3]  ",
	"          ] [            st[4]           ] [    ",
	"       st[5]          ]  [             st[6]    ",
    "    ] [           st[7]            ] [sts] [mgc]",
    "[         ] [         ]  [         ] [         ]",
    "[         ] [         ]  [  mxcsr  ] [ reserved]",
    "[      _fxsr_st[0]       <   exp  >] [ padding ]",
    "[      _fxsr_st[1]       <   exp  >] [ padding ]",
    "[      _fxsr_st[2]       <   exp  >] [ padding ]",
    "[      _fxsr_st[3]       <   exp  >] [ padding ]",
    "[      _fxsr_st[4]       <   exp  >] [ padding ]",
    "[      _fxsr_st[5]       <   exp  >] [ padding ]",
    "[      _fxsr_st[6]       <   exp  >] [ padding ]",
    "[      _fxsr_st[7]       <   exp  >] [ padding ]",
    "[                   _xmm[0]                    ]",
    "[                   _xmm[1]                    ]",
    "[                   _xmm[2]                    ]",
    "[                   _xmm[3]                    ]",
    "[                   _xmm[4]                    ]",
    "[                   _xmm[5]                    ]",
    "[                   _xmm[6]                    ]",
    "[                   _xmm[7]                    ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",
    "[                                              ]",

};
static inline long min(long a, long b) { return a<b ? a:b; }

void printhex_taint_highlight(const void *data, int len, const void *taint, int offset,
                              const void *highlight, int highlight_len, const char *description[])
{
	ssize_t row, i;
	char d[16];
	const char *color[] =
	{
		[0] = "\033[0;37m", [1] = "\033[0;31m",
		[2] = "\033[1;37m", [3] = "\033[1;31m"
	};

	for (row=0; row*16<len; row++)
	{
		for (i=0; i<min(len-row,16); i++)
		{
			d[i] = ((char *)taint)[row*16+i] ? 1 : 0;
			if (contains(highlight, highlight_len, &((const char *)data)[row*16+i]) )
				d[i] |= 2;
		}

		hexdump_line(out, (char*)data+row*16, min(16, len-row*16), offset, 1, description ? description[row] : NULL, d, color);
	}
}

void printhex_taint(const void *data, int len, const void *taint)
{
	printhex_taint_highlight(data, len, taint, 0, NULL, 0, NULL);
}

void printhex_taint_off(const void *data, int len, const void *taint)
{
	printhex_taint_highlight(data, len, taint, 1, NULL, 0, NULL);
}

#ifdef EMU_DEBUG
void print_debug_data(void)
{
	debug("counts/misses ijmp: %u/%u", ijmp_count, ijmp_misses);
}

void print_last_gencode_opcode(void)
{
	char *jit_op, *op;
	long jit_op_len, op_len;
	op = jit_rev_lookup_addr(last_jit, &jit_op, &jit_op_len);
	op_len = op_size(op, 16);
	fd_printf(out, "last opcode at: %X %d\n", op, op_len);
	hexdump(out, op, op_len, 1, 0, NULL,NULL,NULL);
	fd_printf(out, "last jit opcode at: %X\n", last_jit);
	hexdump(out, jit_op, jit_op_len, 1, 0, NULL,NULL,NULL);
}

#endif

static unsigned long read_addr(char *s)
{
	unsigned long addr = 0;
	int i;
	for(i=0; i<8; i++)
	{
		addr *= 16;
		switch (s[i])
		{
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				addr += 9;
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				addr += (s[i]&0xf);
				continue;
			default:
				die("not an address");
		}
	}
	return addr;
}

void dump_map(char *addr, unsigned long len)
{
	long *laddr;
	unsigned long i,j, last=0xFFFFFFFF;
	int t;
	for (i=0; i<len; i+=PG_SIZE)
	{
		t=0;
		laddr = (long *)&addr[i+TAINT_OFFSET];
		for (j=0; j<PG_SIZE/sizeof(long); j++)
			if (laddr[j])
				t=1;

		if (t)
		{
			if (last == 0xFFFFFFFF)
				fd_printf(out, "in map: %x (size %u)\n", addr, len);
			else if (i != last+PG_SIZE)
				fd_printf(out, "...\n");

			printhex_taint_off(&addr[i], PG_SIZE, &addr[i+TAINT_OFFSET]);
			last = i;
		}
	}
}

void do_taint_dump(long *regs)
{
	unsigned long s_addr, e_addr;
	char buf[8];
	int fd = sys_open("/proc/self/maps", O_RDONLY, 0);
	char name[64] = "taint_hexdump_";
	hexcat(name, sys_gettid());
	strcat(name, ".dump");
	int fd_out = sys_open(name, O_RDWR|O_CREAT, 0600), old_out;

	old_out = out;
	out = fd_out;

	fd_printf(out, "tainted jump address:\n");

	printhex_taint(&user_eip, 4, &ijmp_taint);

	fd_printf(out, "registers:\n");

	char regs_taint[32];
	get_xmm6(&regs_taint[0]);
	get_xmm7(&regs_taint[16]);
	printhex_taint_highlight(regs, 32, regs_taint, 0, NULL, 0, regs_desc);


#ifdef EMU_DEBUG
	print_last_gencode_opcode();
#endif

	do
	{
		sys_read(fd, buf, 8);
		s_addr = read_addr(buf);
		sys_read(fd, buf, 1);
		sys_read(fd, buf, 8);
		e_addr = read_addr(buf);
		sys_read(fd, buf, 2);
		sys_read(fd, buf, 1);
		if (buf[0] == 'w')
		{
			if (e_addr > USER_END)
				e_addr = USER_END;
			dump_map((char *)s_addr, e_addr-s_addr);
		}
		while (sys_read(fd, buf, 1) && buf[0] != '\n');
	}
	while (e_addr < USER_END);

	out = old_out;

	sys_close(fd);
}

void print_sigcontext(struct sigcontext *sc)
{
	hexdump(out, sc, sizeof(*sc), 0, 1, sigcontext_desc, NULL, NULL);
}

void print_sigcontext_diff(struct sigcontext *sc1, struct sigcontext *sc2)
{
	hexdump_diff(out, sc1, sizeof(*sc1), sc2, sizeof(*sc2), sizeof(long), 0, 1, sigcontext_desc);
}

void print_fpstate(struct _fpstate *fpstate)
{
	hexdump(out, fpstate, sizeof(*fpstate), 0, 1, fpstate_desc, NULL, NULL);
}

void print_fpstate_diff(struct _fpstate *fpstate1, struct _fpstate *fpstate2)
{
	hexdump_diff(out, fpstate1, sizeof(*fpstate1), fpstate2, sizeof(*fpstate2), sizeof(long), 0, 1, fpstate_desc);
}

void print_siginfo(siginfo_t *info)
{
	hexdump(out, info, sizeof(*info), 0, 1, NULL, NULL, NULL);
}

void print_siginfo_diff(siginfo_t *info1, siginfo_t *info2)
{
	hexdump_diff(out, info1, sizeof(*info1), info2, sizeof(*info2), sizeof(long), 1, 1, NULL);
}

void print_ucontext(struct kernel_ucontext *uc)
{
	hexdump(out, uc, offsetof(struct kernel_ucontext, uc_mcontext), 0, 1, ucontext_pre_desc, NULL, NULL);
	print_sigcontext(&uc->uc_mcontext);
	hexdump(out, &uc->uc_sigmask, sizeof(uc->uc_sigmask), 0, 1, ucontext_post_desc, NULL, NULL);
}

void print_ucontext_diff(struct kernel_ucontext *uc1, struct kernel_ucontext *uc2)
{
	hexdump_diff(out, uc1, offsetof(struct kernel_ucontext, uc_mcontext),
	                  uc2, offsetof(struct kernel_ucontext, uc_mcontext),
	                  sizeof(long), 0, 1, ucontext_pre_desc);
	print_sigcontext_diff(&uc1->uc_mcontext, &uc2->uc_mcontext);
	hexdump_diff(out, &uc1->uc_sigmask, sizeof(uc1->uc_sigmask),
	                  &uc2->uc_sigmask, sizeof(uc2->uc_sigmask),
	                  sizeof(long), 0, 1, ucontext_post_desc);
}

void print_rt_sigframe(struct kernel_rt_sigframe *frame)
{
	debug("start: %x, end: %x, size: %x", frame, &frame[1], sizeof(*frame));
	debug("@ %x", frame);
	hexdump(out, frame, 16, 0, 1, rt_sigframe_pre_desc, NULL, NULL);
	debug("@ %x", &frame->info);
	print_siginfo(&frame->info);
	debug("@ %x", &frame->uc);
	print_ucontext(&frame->uc);
	debug("@ %x", &frame->fpstate);
	print_fpstate(&frame->fpstate);
	debug("@ %x", &frame->retcode);
	hexdump(out, &frame->retcode, 8, 0, 1, rt_sigframe_post_desc, NULL, NULL);
}

void print_rt_sigframe_diff(struct kernel_rt_sigframe *frame1, struct kernel_rt_sigframe *frame2)
{
	hexdump_diff(out, frame1, 16, frame2, 16, sizeof(long), 0, 1, rt_sigframe_pre_desc);
	print_siginfo_diff(&frame1->info, &frame2->info);
	print_ucontext_diff(&frame1->uc, &frame2->uc);
	print_fpstate_diff(&frame1->fpstate, &frame2->fpstate);
	hexdump_diff(out, &frame1->retcode, 8, &frame2->retcode, 8, sizeof(long), 0, 1, rt_sigframe_post_desc);
}

void print_sigframe(struct kernel_sigframe *frame)
{
	debug("start: %x, end: %x, size: %x", frame, &frame[1], sizeof(*frame));
	debug("@ %x", frame);
	hexdump(out, frame, 8, 0, 1, sigframe_pre_desc, NULL, NULL);
	debug("@ %x", &frame->sc);
	print_sigcontext(&frame->sc);
	debug("@ %x", &frame->fpstate);
	print_fpstate(&frame->fpstate);
	debug("@ %x", &frame->extramask);
	hexdump(out, &frame->extramask, 12, 0, 1, sigframe_post_desc, NULL, NULL);
}

void print_sigframe_diff(struct kernel_sigframe *frame1, struct kernel_sigframe *frame2)
{
	hexdump_diff(out, frame1, 8, frame2, 8, sizeof(long), 0, 1, sigframe_pre_desc);
	print_sigcontext_diff(&frame1->sc, &frame2->sc);
	print_fpstate_diff(&frame1->fpstate, &frame2->fpstate);
	hexdump_diff(out, &frame1->extramask, 12, &frame2->extramask, 12, sizeof(long), 0, 1, sigframe_post_desc);
}

#define _LARGEFILE64_SOURCE 1
#include <asm/stat.h>

void print_stat(const struct stat64 *s)
{
	hexdump(out, s, sizeof(*s), 0, 0, stat64_desc, NULL, NULL);
}


