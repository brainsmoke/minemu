
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	debug("@ %x", frame->uc.uc_mcontext.fpstate);
	print_fpstate(frame->uc.uc_mcontext.fpstate);
	debug("@ %x", &frame->retcode);
	hexdump(out, &frame->retcode, 8, 0, 1, rt_sigframe_post_desc, NULL, NULL);
}

void print_rt_sigframe_diff(struct kernel_rt_sigframe *frame1, struct kernel_rt_sigframe *frame2)
{
	hexdump_diff(out, frame1, 16, frame2, 16, sizeof(long), 0, 1, rt_sigframe_pre_desc);
	print_siginfo_diff(&frame1->info, &frame2->info);
	print_ucontext_diff(&frame1->uc, &frame2->uc);
	print_fpstate_diff(frame1->uc.uc_mcontext.fpstate, frame2->uc.uc_mcontext.fpstate);
	hexdump_diff(out, &frame1->retcode, 8, &frame2->retcode, 8, sizeof(long), 0, 1, rt_sigframe_post_desc);
}

void print_sigframe(struct kernel_sigframe *frame)
{
	debug("start: %x, end: %x, size: %x", frame, &frame[1], sizeof(*frame));
	debug("@ %x", frame);
	hexdump(out, frame, 8, 0, 1, sigframe_pre_desc, NULL, NULL);
	debug("@ %x", &frame->sc);
	print_sigcontext(&frame->sc);
	debug("@ %x", frame->sc.fpstate);
	print_fpstate(frame->sc.fpstate);
	debug("@ %x", &frame->extramask);
	hexdump(out, &frame->extramask, 12, 0, 1, sigframe_post_desc, NULL, NULL);
}

void print_sigframe_diff(struct kernel_sigframe *frame1, struct kernel_sigframe *frame2)
{
	hexdump_diff(out, frame1, 8, frame2, 8, sizeof(long), 0, 1, sigframe_pre_desc);
	print_sigcontext_diff(&frame1->sc, &frame2->sc);
	print_fpstate_diff(frame1->sc.fpstate, frame2->sc.fpstate);
	hexdump_diff(out, &frame1->extramask, 12, &frame2->extramask, 12, sizeof(long), 0, 1, sigframe_post_desc);
}

void print_stat(const struct kernel_stat64 *s)
{
	hexdump(out, s, sizeof(*s), 0, 0, stat64_desc, NULL, NULL);
}


