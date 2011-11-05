
# This file is part of minemu
#
# Copyright 2010-2011 Erik Bosman <erik@minemu.org>
# Copyright 2011 Vrije Universiteit Amsterdam
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

#include <asm/unistd.h>
#define SIGKILL      9
#include "asm_consts_gen.h"

.text
.global mutex_lock # ( long *lock_addr )
.type mutex_lock, @function
mutex_lock:
push %ebx
mutex_lock_retry:
movl $1, %edx
xor %ecx, %ecx
movl 8(%esp), %ebx
spin:
movl $0, %eax
lock cmpxchg %edx, (%ebx)
loopnew spin
jne 1f
pop %ebx
lfence
ret
1:
movl $(__NR_sched_yield), %eax
int $0x80
jmp mutex_lock_retry

.global mutex_init # ( long *lock_addr )
.type mutex_init, @function
mutex_init:

.global mutex_unlock # ( long *lock_addr )
.type mutex_unlock, @function
mutex_unlock:
movl 4(%esp), %edx
sfence
movl $0, (%edx)
ret

.global mutex_unlock_exit # ( long status, long *lock_addr )
.type mutex_unlock_exit, @function
mutex_unlock_exit:
movl 8(%esp), %edx
movl 4(%esp), %ebx
movl $(__NR_exit), %eax
sfence
movl $0, (%edx)
int $0x80
ud2

.global mutex_unlock_execve_or_die # ( char *filename, char *argv[], char *envp[], long *lock_addr )
.type mutex_unlock_execve_or_die, @function
mutex_unlock_execve_or_die:
movl 0x10(%esp), %esi
movl 0x0C(%esp), %edx
movl 0x08(%esp), %ecx
movl 0x04(%esp), %ebx
movl $(__NR_execve), %eax
sfence
movl $0, (%esi)
int $0x80
movl $(__NR_gettid), %eax
int $0x80
movl %eax, %ebx
movl %eax, %ecx
movl $(SIGKILL), %edx
movl $(__NR_tgkill), %eax
int $0x80
ud2

# This wil break HARD with -fomit-frame-pointers
#
#
patch_base_pointers: # ( difference )
test %ebp, %ebp
jz done
addl 4(%esp), %ebp
movl %ebp, %edx

1:
movl (%edx), %eax
test %eax, %eax
jz done
addl 4(%esp), %eax
movl %eax, (%edx)
movl %eax, %edx
jmp 1b

done:
ret


.global clone_relocate_stack # ( flags, sp, &parent_tid, dummy, &child_tid, stack_diff )
.type clone_relocate_stack, @function
clone_relocate_stack:
push %ebp
movl %esp, %ebp
push %ebx
push %esi
push %edi

movl 0x1c(%ebp), %eax
push %eax                  # stack_diff (for patch_base_pointers)

movl %esp, %ebx            # src
addl %esp, %eax            # dest ( = stack_diff + src )

movl %fs:CTX__MY_ADDR, %ecx
addl $CTX__SIZE, %ecx
subl %esp, %ecx            # size

push %ecx
push %ebx
push %eax
call memcpy
lea 0x0c(%esp), %esp

movl $(__NR_clone), %eax
movl 0x08(%ebp), %ebx                          # flags
movl 0x1c(%ebp), %ecx                          # child_sp = stack_diff
addl      %esp , %ecx                          # child_sp = stack_diff + sp
movl 0x10(%ebp), %edx                          # &parent_tid
movl 0x14(%ebp), %esi
movl 0x18(%ebp), %edi                          # &child_tid
int $0x80
cmp %ecx, %esp
jne 1f
mov %eax, %ebx
call patch_base_pointers # ( stack_diff )
mov %ebx, %eax

1:
lea 4(%esp), %esp          # discard stack_diff
pop %edi
pop %esi
pop %ebx
pop %ebp
ret
