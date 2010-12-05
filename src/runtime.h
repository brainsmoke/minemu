
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

#ifndef RUNTIME_H
#define RUNTIME_H

#include "scratch.h"

void emu_start(void *eip, long *esp);
void state_restore(void);

long runtime_ijmp(void);
long runtime_ret_cleanup(void);
long jit_fragment_exit(void);

long int80_emu(void);
long linux_sysenter_emu(void);

#endif /* RUNTIME_H */
