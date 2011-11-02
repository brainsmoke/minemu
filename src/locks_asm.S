
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
movl $0, (%edx)
ret
