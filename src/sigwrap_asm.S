
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

.global load_sigframe
.type load_sigframe, @function
load_sigframe:
movl 4(%esp), %eax
movl 8(%esp), %esp
lea  8(%esp), %esp
int $0x80
ud2

.global load_rt_sigframe
.type load_rt_sigframe, @function
load_rt_sigframe:
movl 4(%esp), %eax
movl 8(%esp), %esp
lea  4(%esp), %esp
int $0x80
ud2

.global get_xmm5
.type get_xmm5, @function
get_xmm5:
movl 4(%esp), %eax
movups %xmm5, (%eax)
ret

.global get_xmm6
.type get_xmm6, @function
get_xmm6:
movl 4(%esp), %eax
movups %xmm6, (%eax)
ret

.global get_xmm7
.type get_xmm7, @function
get_xmm7:
movl 4(%esp), %eax
movups %xmm7, (%eax)
ret

