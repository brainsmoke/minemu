
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TAINT_H
#define TAINT_H

enum
{
	TAINT_ON,
	TAINT_OFF,
};

extern int taint_flag;

void taint_mem(void *mem, unsigned long size, int type);
void do_taint(long ret, long call, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

#endif /* TAINT_H */
