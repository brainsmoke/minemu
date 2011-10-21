
# This file is part of minemu
#
# Copyright 2010-2011 Erik Bosman <erik@minemu.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#ifdef EMU_DEBUG

#define DEBUG_COUNT_IJMP \
xchg %eax, ijmp_count    \
lea 1(%eax), %eax        \
xchg %eax, ijmp_count

#define DEBUG_COUNT_IJMP_MISSES \
incl ijmp_misses

#else

#define DEBUG_COUNT_IJMP        # do nothing
#define DEBUG_COUNT_IJMP_MISSES # do nothing

#endif

.text
#include "scratch.h"

#
# emu_start(): Protect minemu memory, load registers, jump to address
#
.global emu_start
.type emu_start, @function
emu_start: # (long eip, long esp)
movl 4(%esp), %esi           # load user %eip from arg1
movl 8(%esp), %ebp           # load user's stack pointer from arg2

movl $scratch_stack, %esp    # move the stack out of emulator memory
call shield                  # protect emulator memory
movl %ebp, %esp              # load user stack

movl %esi, %eax              # user %eip
xor %ecx, %ecx               # clear registers and flags
xor %edx, %edx
xor %ebx, %ebx
xor %ebp, %ebp
xor %esi, %esi
xor %edi, %edi
pinsrd $0, %ecx, %xmm4
pinsrd $0, %ecx, %xmm3 # cleared %eax

jmp runtime_ijmp

taint_fault:
movl %ecx, ijmp_taint
movl %eax, user_eip
pextrd $0, %xmm4, %ecx
pextrd $0, %xmm3, %eax
pextrd $0, %xmm5, %edx
mov %esp, scratch_stack
mov $scratch_stack, %esp
push %edi
push %esi
push %ebp
push scratch_stack   # user %esp
push %ebx
push %edx
push %ecx
push %eax
push %esp           # *(long)regs
call unshield
call do_taint_dump
#int $3
ud2

#
# RET / ICALL / IJMP FAST PATH:
#
# runtime_ret()/runtime_ijmp() are called from the jit code
#
.balign 64
.global runtime_ret
.type runtime_ret, @function

runtime_ret:
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov .jit_end(%esp), %ecx # .jit_end == TAINT_OFFSET
pop %eax

#
# Status from here: original %eax:         %xmm3[0:31]
#                   original %ecx:         %xmm4[0:31]
#                   taint:                 %ecx
#                   original jump target:  %eax
#
.global runtime_ijmp
.type runtime_ijmp, @function
runtime_ijmp:
DEBUG_COUNT_IJMP

pinsrd $0, %edx, %xmm5
movzwl %ax, %edx
jecxz,pt taint_ok
jmp taint_fault
taint_ok:
movl jmp_cache(, %edx, 8), %ecx
movl jmp_cache+4(, %edx, 8), %edx
lea (%ecx,%eax,1), %ecx
movl %edx, jit_eip
loop cache_lookup
jit_return: # jit_eip contains jump address
pextrd $0, %xmm4, %ecx
pextrd $0, %xmm3, %eax
pextrd $0, %xmm5, %edx
jmp *jit_eip            #  self-modifying code replaces this for jit_fragment and back
                        #  in case of a signal (*UGLY*)
.global runtime_exit_jmpaddr

runtime_exit_jmpaddr:
#
# END FAST PATH
#
#

cache_lookup:
DEBUG_COUNT_IJMP_MISSES

mov %esp, scratch_stack
mov $scratch_stack, %esp
pushf
movzwl %ax, %edx                  # HASH_INDEX[addr]
cache_lookup_loop:
inc %dx
mov jmp_cache(, %edx, 8), %ecx
lea -1(%ecx,%eax,1), %ecx
jecxz,pt cache_hit
testl $-1, jmp_cache(, %edx, 8)   # jmp_cache.addr[HASH_INDEX[addr]] == 0
jz cache_miss
cmp %ax, %dx
jne cache_lookup_loop
jmp cache_miss

cache_hit:
mov jmp_cache+4(, %edx, 8), %edx
movl %edx, jit_eip
lookup_return:
popf
pop %esp
jmp jit_return

# address not found in jmp_cache
#
# Processor state:
#
# %eax:    address
# %e[cd]x: clobbered
#
cache_miss:
push %eax            # addr
push %eax            # addr
call jit_lookup_addr # (char *addr);
lea 4(%esp), %esp
test %eax, %eax      # jit_addr or NULL
jnz lookup_hit

# the code we're looking for has not been translated yet
call runtime_jit     # (char *addr);

lookup_hit:
lea 4(%esp), %esp
movl %eax, jit_eip
jmp lookup_return

#
# total miss, we have to translate some code
#
runtime_jit:
call unshield                       # unprotect jit stack
movl 4(%esp), %eax
movl %esp, minemu_stack_bottom      # switch to jit stack
movl $minemu_stack_bottom, %esp     #
push %eax                           # address to be translated
call jit
addl $4, %esp
pop %esp                            # revert to scratch stack
push %eax
call shield                         # protect jit stack
pop %eax                            # translated address is stored in %eax
ret

#
# syscall wrapper
#
runtime_syscall:
pushf
push %ecx
push %edx
push %ebp
push %edi
push %esi
push %edx
push %ecx
push %ebx
push %eax
call syscall_emu
lea 28(%esp), %esp
pop %edx
pop %ecx
popf
ret

.global int80_emu
.type int80_emu, @function
int80_emu:
movl %esp, scratch_stack
movl $scratch_stack, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl user_eip, %eax
jmp runtime_ijmp

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
movl (%esp), %ebp      # restore %ebp, (is used to store %esp during sysenter.)
movl %esp, scratch_stack
movl $scratch_stack, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl sysenter_reentry, %eax
jmp runtime_ijmp

.global state_restore
.type state_restore, @function
state_restore:
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl user_eip, %eax
jmp runtime_ijmp

