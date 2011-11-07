
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
#include "segments.h"

#
# syscall wrapper
#
runtime_syscall:
pushf
push %ecx
push %edx
push %ebp
xor %ebp, %ebp
push %edi
push %esi
push %edx
push %ecx
push %ebx
push %eax
call syscall_emu
lea 24(%esp), %esp
pop %ebp
pop %edx
pop %ecx
popf
ret

#
# emu_start(): Protect minemu memory, load registers, jump to address
#
.global emu_start
.type emu_start, @function
emu_start: # (long eip, long esp)
movl %esp, minemu_stack_bottom   # save orig stack address for runtime_jit()
movl 4(%esp), %eax               # load user %eip from arg1
movl 8(%esp), %esp               # load user's stack pointer from arg2
xor %ecx, %ecx                   # clear registers and flags
xor %edx, %edx
xor %ebx, %ebx
xor %ebp, %ebp
xor %esi, %esi
xor %edi, %edi
pinsrd $0, %ecx, %xmm4
pinsrd $0, %ecx, %xmm3 # cleared %eax
SHIELDS_UP
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global int80_emu
.type int80_emu, @function
int80_emu:
SHIELDS_DOWN
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl %fs:CTX__USER_EIP, %eax
SHIELDS_UP
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
SHIELDS_DOWN
movl (%esp), %ebp      # restore %ebp, (is used to store %esp during sysenter.)
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl sysenter_reentry, %eax
SHIELDS_UP
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global state_restore
.type state_restore, @function
state_restore:
SHIELDS_DOWN
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl %fs:CTX__USER_EIP, %eax
SHIELDS_UP
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

