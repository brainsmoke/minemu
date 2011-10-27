
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

#include "scratch.h"
#include "asm_consts_gen.h"
.bss
#.balign 4096 # .{b,p2,}align doesn't seem to work
.lcomm ctx, CTX_ARRAY__SIZE
.global ctx

.lcomm fault_page_1, 4096
.global fault_page_1

.lcomm scratch_stack_bottom, 32764             # runtime stack
.global scratch_stack_bottom                   #
.lcomm scratch_stack, 4                        #
.global scratch_stack                          #

.lcomm fault_page_2, 4096
.global fault_page_2

.lcomm fault_page_3, 4096
.global fault_page_3

.lcomm user_eip, 4
.global user_eip
.lcomm jit_eip, 4
.global jit_eip
