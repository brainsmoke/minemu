
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
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

#ifndef JIT_MM_H
#define JIT_MM_H

void jit_mem_init(void);

void jit_mem_free(void *p);
void *jit_mem_balloon(void *p); /* get largest possible memory region */
unsigned long jit_mem_size(void *p);
unsigned long jit_mem_try_resize(void *p, unsigned long requested_size);

#endif /* JIT_MM_H */
