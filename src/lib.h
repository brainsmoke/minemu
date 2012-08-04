
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

#ifndef LIB_H
#define LIB_H

#include <unistd.h>
#include <stdarg.h>

#undef isprint
int isprint(int c);

int fd_vprintf(int fd, const char *format, va_list ap);
int fd_printf(int fd, const char *format, ...);

char *getenve(const char *name, char **environment);

long read_at(int fd, off_t off, void *buf, size_t size);
char *numcat(char *dest, long l);
char *hexcat(char *dest, unsigned long ul);

unsigned long hexread(const char *s);
unsigned long numread(const char *s);

unsigned long long strtohexull(char *s, char **end);

void copy_cmdline(char **dest, char **src);

/* dest is assumed to be a buffer of at least PATH_MAX+1 bytes */
int absdir(char *dest, const char *dir);

long memscan(const char *hay, long haylen, const char *needle, long needlelen);

void clear(void *buf, size_t n);

static inline int overlap(const char *addr1, unsigned long len1, const char *addr2, unsigned long len2)
{
	return ((unsigned long)addr1 < (unsigned long)addr2+len2) &&
	       ((unsigned long)addr2 < (unsigned long)addr1+len1);
}

static inline int between(const char *start, char *end, char *addr)
{
	return ((unsigned long)addr >= (unsigned long)start) &&
	       ((unsigned long)addr <= (unsigned long)end);
}

static inline int contains(const char *addr1, unsigned long len1, const char *addr2)
{
	return overlap(addr1, len1, addr2, 1);
}

#endif /* LIB_H */
