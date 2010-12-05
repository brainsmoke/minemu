
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

#ifndef CODEMAP_H
#define CODEMAP_H

typedef struct
{
    char *addr; 
    unsigned long len;
    char *jit_addr;
	unsigned long jit_len;

	/* mmapped file attributes */
	unsigned long long inode, dev;
	unsigned long mtime, pgoffset;

} code_map_t;

code_map_t *find_code_map(char *addr);
code_map_t *find_jit_code_map(char *jit_addr);

void add_code_region(char *addr, unsigned long len, unsigned long long inode,
                                                    unsigned long long dev,
                                                    unsigned long mtime,
                                                    unsigned long pgoffset);

void del_code_region(char *addr, unsigned long len);

#endif /* CODEMAP_H */
