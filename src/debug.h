
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
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

#ifndef DEBUG_H
#define DEBUG_H

#include "sigwrap.h"

void print_sigcontext(struct sigcontext *sc);
void print_fpstate(struct _fpstate *fpstate);
void print_siginfo(siginfo_t *info);
void print_ucontext(struct kernel_ucontext *uc);
void print_rt_sigframe(struct kernel_rt_sigframe *frame);
void print_sigframe(struct kernel_sigframe *frame);

void print_sigcontext_diff(struct sigcontext *sc1, struct sigcontext *sc2);
void print_fpstate_diff(struct _fpstate *fpstate1, struct _fpstate *fpstate2);
void print_siginfo_diff(siginfo_t *info1, siginfo_t *info2);
void print_ucontext_diff(struct kernel_ucontext *uc1, struct kernel_ucontext *uc2);
void print_rt_sigframe_diff(struct kernel_rt_sigframe *frame1, struct kernel_rt_sigframe *frame2);
void print_sigframe_diff(struct kernel_sigframe *frame1, struct kernel_sigframe *frame2);

struct stat64;
void print_stat(const struct stat64 *s);

#ifdef EMU_DEBUG
void print_debug_data(void);

void print_last_gencode_opcode(void);

#endif

#endif /* DEBUG_H */
