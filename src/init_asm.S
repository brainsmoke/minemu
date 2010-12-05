
# This file is part of minemu
#
# Copyright 2010 Erik Bosman <erik@minemu.org>
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
.global start
.type start, @function
start:
pop  %edx                   # argc -> %edx, argv -> %esp
lea  4(%esp, %edx, 4), %ebx # envp -> %ebx
xor  %eax,             %eax
xor  %ecx,             %ecx
dec  %ecx
cld
mov  %ebx,             %edi
repne scas %es:(%edi), %eax # auxv -> %edi
mov  %esp,             %eax # argv -> %eax
push %edi                   # push auxv
push %ebx                   # push envp
push %eax                   # push argv
push %edx                   # push argc
call minemu_main
