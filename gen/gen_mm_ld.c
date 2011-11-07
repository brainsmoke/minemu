
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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <assert.h>

#include "mm.h"
#include "threads.h"

/* export some of our nice preprocessor defines to our linker file */

int main(void)
{
	printf("minemu_start = 0x%lx;\n", MINEMU_START);
	printf("taint_offset = 0x%lx;\n", TAINT_OFFSET);
	printf("offset__jit_fragment_exit_addr = 0x%lx;\n", offsetof(thread_ctx_t, jit_fragment_exit_addr));
	printf("offset__jit_eip = 0x%lx;\n", offsetof(thread_ctx_t, jit_eip));

	exit(EXIT_SUCCESS);
}
