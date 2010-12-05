
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


.global jit_fragment_run
.type jit_fragment_run, @function
jit_fragment_run:
incl jit_fragment_running
movl $0, jit_fragment_restartsys
movl  4(%esp), %eax                  # (struct sigcontext *)%eax
pusha
pushf
push %eax                            # (struct sigcontext *)%eax
movl 0x38(%eax), %edx                # (struct sigcontext *)%eax->eip
movl %edx, jit_fragment_scratch
movl 0x1C(%eax), %ebp
mov %esp, jit_fragment_saved_esp

# %eax points to a struct sigcontext
# %esp from sigcontext is ignored, %esp is set to %ebp
# %eip from sigcontext is ignored, we'll jump to the value in jit_eip

push 0x40(%eax)
popf
movl %ebp, %esp

movl 0x10(%eax), %edi
movl 0x14(%eax), %esi
movl 0x18(%eax), %ebp
#ignored:  movl 0x1C(%eax), %esp
movl 0x20(%eax), %ebx
movl 0x24(%eax), %edx
movl 0x28(%eax), %ecx
movl 0x2C(%eax), %eax

jmp *jit_fragment_scratch

.global jit_fragment_exit
.type jit_fragment_exit, @function
jit_fragment_exit:
xchg jit_fragment_saved_esp, %esp

xchg %eax, (%esp)                    # (struct sigcontext *)%eax / user %eax
pushf

# restore sigcontext
movl %edi, 0x10(%eax)
movl %esi, 0x14(%eax)
movl %ebp, 0x18(%eax)
movl jit_fragment_saved_esp, %ebp
movl %ebp, 0x1C(%eax)                # (struct sigcontext *)%eax->esp
movl %ebp, 0x44(%eax)                # (struct sigcontext *)%eax->esp@sig
movl %ebx, 0x20(%eax)
movl %edx, 0x24(%eax)
movl %ecx, 0x28(%eax)
pop %edx                             # flags
movl %edx, 0x40(%eax)
pop %edx                             # user %eax
movl %edx, 0x2C(%eax)
movl jit_eip, %ebp                   # %eip
movl %ebp, 0x38(%eax)                # (struct sigcontext *)%eax->eip

popf
popa
decl jit_fragment_running
ret

