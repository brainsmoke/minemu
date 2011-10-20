
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

#include "scratch.h"
.bss
#.balign 4096 # .{b,p2,}align doesn't seem to work

.lcomm fault_page_0, 4096
.global fault_page_0

.lcomm sigwrap_stack_bottom, 32764             # signal stack
.global sigwrap_stack_bottom                   #
.lcomm sigwrap_stack, 4                        #
.global sigwrap_stack                          #

.lcomm fault_page_1, 4096
.global fault_page_1

.lcomm scratch_stack_bottom, 32764             # runtime stack
.global scratch_stack_bottom                   #
.lcomm scratch_stack, 4                        #
.global scratch_stack                          #

.lcomm fault_page_2, 4096
.global fault_page_2

.lcomm jit_fragment_page, 4096
.global jit_fragment_page

.lcomm fault_page_3, 4096
.global fault_page_3

.lcomm jmp_cache, JMP_CACHE_MEM_SIZE
.global jmp_cache

.global taint_tmp
.lcomm taint_tmp, 4

.global ijmp_taint
.lcomm ijmp_taint, 4

.lcomm jit_fragment_running, 4
.global jit_fragment_running
.lcomm jit_fragment_restartsys, 4
.global jit_fragment_restartsys
.lcomm jit_fragment_saved_esp, 4
.global jit_fragment_saved_esp
.lcomm jit_fragment_scratch, 4
.global jit_fragment_scratch

.lcomm old_sigset, 8
.global old_sigset

.lcomm user_eip, 4
.global user_eip
.lcomm jit_eip, 4
.global jit_eip
.lcomm sysenter_reentry, 4
.global sysenter_reentry

.lcomm fd_type, 1024
.global fd_type 

#ifdef EMU_DEBUG
.lcomm ijmp_misses, 4
.global ijmp_misses
.lcomm ijmp_count, 4
.global ijmp_count
.lcomm last_jit, 4
.global last_jit
#endif

