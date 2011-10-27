
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

#
# emu_start(): Protect minemu memory, load registers, jump to address
#
.global emu_start
.type emu_start, @function
emu_start: # (long eip, long esp)
movl 4(%esp), %esi           # load user %eip from arg1
movl 8(%esp), %ebp           # load user's stack pointer from arg2

mov %fs:CTX__SCRATCH_STACK_TOP, %esp # move the stack out of emulator memory
call shield                          # protect emulator memory
movl %ebp, %esp                      # load user stack

movl %esi, %eax              # user %eip
xor %ecx, %ecx               # clear registers and flags
xor %edx, %edx
xor %ebx, %ebx
xor %ebp, %ebp
xor %esi, %esi
xor %edi, %edi
pinsrd $0, %ecx, %xmm4
pinsrd $0, %ecx, %xmm3 # cleared %eax
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global int80_emu
.type int80_emu, @function
int80_emu:
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl %fs:CTX__USER_EIP, %eax
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
movl (%esp), %ebp      # restore %ebp, (is used to store %esp during sysenter.)
mov %esp, %fs:CTX__USER_ESP
mov %fs:CTX__SCRATCH_STACK_TOP, %esp
call runtime_syscall
pop %esp
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl sysenter_reentry, %eax
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

.global state_restore
.type state_restore, @function
state_restore:
pinsrd $0, %ecx, %xmm4
pinsrd $0, %eax, %xmm3
mov $0x0,%ecx
movl %fs:CTX__USER_EIP, %eax
jmp *%fs:CTX__RUNTIME_IJMP_ADDR

