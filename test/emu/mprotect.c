
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


#include <sys/mman.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

char *get_rwmem(size_t sz)
{
	return mmap(NULL, sz, PROT_READ|PROT_WRITE,
	                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

int main(void)
{
	char c[1];
	char *reg = get_rwmem(40960);

	mprotect(&reg[4096], 4096, PROT_NONE);

	read(0, c, 1);
}
