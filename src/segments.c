
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 * Copyright 2011 Vrije Universiteit Amsterdam
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

#include <asm/ldt.h>

#include "segments.h"
#include "syscalls.h"

long shield_segment = SHIELD_SEGMENT, data_segment;

static void set_fs_segment(int number)
{
	__asm__ __volatile__ ("mov %0, %%fs":: "r" (number));
}

static void create_segment(int entry_number, void *base_addr, unsigned long size)
{
	struct user_desc seg =
	{
		.entry_number = entry_number,
		.base_addr = (int)base_addr,
		.limit = 1 + ( (size-1) & ~0xfff ) / 0x1000,
		.seg_32bit = 1,
		.limit_in_pages = 1,
	};

	sys_set_thread_area(&seg);
}

void init_tls(void *base_addr, unsigned long size)
{
	create_segment(TLS_GDT_ENTRY, base_addr, size);
	set_fs_segment(TLS_SEGMENT);
}

/* The segment shield that makes emulated code unable to touch emulator memory
 */
void init_shield(unsigned long size)
{
	create_segment(SHIELD_GDT_ENTRY, 0x00000000, size);
	data_segment = 0;
	__asm__ __volatile__ ("mov %%ds, data_segment"::); /* save original data segment */
}
