
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

#ifndef PROC_H
#define PROC_H

void fake_proc_self_stat(long fd);

/* simple /proc/self/maps parser: */

typedef struct
{
	char buf[1024];
	long n_read, i, fd;

} map_file_t;

typedef struct
{
	unsigned long addr, len, prot;

} map_entry_t;

int open_maps(map_file_t *f);
int read_map(map_file_t *f, map_entry_t *e);
int close_maps(map_file_t *f);

#endif /* PROC_H */
