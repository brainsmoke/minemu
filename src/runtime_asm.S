
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

.text
#include "asm_consts_gen.h"
#include "scratch.h"

.global runtime_cache_resolution_start
runtime_cache_resolution_start:

taint_fault:
movl %ecx, %fs:CTX__IJMP_TAINT
movl %eax, %fs:CTX__USER_EIP
pextrd $0, %xmm4, %ecx
pextrd $0, %xmm3, %eax
pextrd $0, %xmm5, %edx
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
push %edi
push %esi
push %ebp
push 12(%esp)       # user %esp
push %ebx
push %edx
push %ecx
push %eax
push %esp           # *(long)regs
call unshield
call do_taint_dump
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
pinsrd $0, %edx, %xmm5
movzwl %ax, %edx
jecxz,pt taint_ok
jmp taint_fault
taint_ok:
movl %fs: (, %edx, 8), %ecx         # load mangled cached address
movl %fs:4(, %edx, 8), %edx
lea (%ecx,%eax,1), %ecx             # %ecx is 1 if there is a cache hit
movl %edx, %fs:CTX__JIT_EIP
loop cache_lookup                   # branch taken on cache miss

.global jit_return
.type jit_return, @function
jit_return: # exec_ctx->jit_eip contains jump address
pextrd $0, %xmm4, %ecx
pextrd $0, %xmm3, %eax
pextrd $0, %xmm5, %edx
jmp *%fs:offset__jit_eip_HACK       #  TODO XXX TODO Comment on relocated code+symbol thing in Makefile
#
# END FAST PATH
#
#

cache_lookup:
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
pushf
movzwl %ax, %edx                  # HASH_INDEX[addr]
cache_lookup_loop:
inc %dx
mov %fs:(, %edx, 8), %ecx
lea -1(%ecx,%eax,1), %ecx
jecxz,pt cache_hit
testl $-1, %fs:(, %edx, 8)   # jmp_cache.addr[HASH_INDEX[addr]] == 0
jz cache_miss
cmp %ax, %dx
jne cache_lookup_loop
jmp cache_miss
cache_hit:
mov %fs:4(, %edx, 8), %edx
movl %edx, %fs:CTX__JIT_EIP
popf
pop %esp
jmp jit_return

.global runtime_cache_resolution_end
runtime_cache_resolution_end:
nop

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
movl %eax, %fs:CTX__JIT_EIP
popf
pop %esp
jmp *%fs:CTX__JIT_RETURN_ADDR

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

