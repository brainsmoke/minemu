
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

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <limits.h>
#include <fcntl.h>

#include "syscalls.h"
#include "error.h"
#include "lib.h"

/* versions of stdlib functions */

int isprint(int c)
{
	return c >= ' ' && c <= '~';
}

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i;
	for (i=0; i<n && (dest[i] = src[i]); i++);
	return dest;
}

char *strcpy(char *dest, const char *src)
{
	size_t i;
	for (i=0; (dest[i]=src[i]); i++);
	return dest;
}

size_t strlen(const char *s)
{
	size_t i;
	for (i=0; s[i]; i++);
	return i;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	size_t i;
	int c=0;
	for (i=0; i<n && ((c=s1[i]-s2[i]) == 0) && s1[i]; i++);
		return c;
}

int strcmp(const char *s1, const char *s2)
{
	size_t i;
	int c;
	for (i=0; ((c=s1[i]-s2[i]) == 0) && s1[i]; i++);
		return c;
}

char *strchr(const char *s, int c)
{
	for (; *s && *s != c; s++);
	if (*s)
		return (char *)s; /* hrmmr :-) */
	else
		return NULL;
}

char *strrchr(const char *s, int c)
{
	char *last=NULL;
	for (;*s; s++)
		if (*s == c)
			last = (char *)s; /* hrmmr :-) */

	return last;
}

int memcmp(const void *v1, const void *v2, size_t n)
{
	const char *s1=v1, *s2=v2;
	size_t i;
	int r;

	for (i=0; i<n; i++)
		if ( (r = s1[i]-s2[i]) != 0 )
			return r;

	return 0;
}

void *memcpy(void *v_dest, const void *v_src, size_t n)
{
	const char *src=v_src;
	char *dest=v_dest;
	size_t i;
	for (i=0; i<n; i++)
		dest[i] = src[i];

	return v_dest;
}

void *memset(void *v, int c, size_t n)
{
	char *s=v;
	size_t i;

	for (i=0; i<n; i++)
		s[i]=c;

	return v;
}

void clear(void *buf, size_t n)
{
	memset(buf, 0, n);
}

/* stdlib-like */

static char *hex = "0123456789abcdef";

/* for base <= 16 */
static int fd_printnum(int fd, unsigned long num, long base, int pad, int numpad)
{
	unsigned long digit=1;
	int n_out = 1;
	char c = pad;

	while ( (num/digit/base) && (ULONG_MAX/base > digit) )
	{
		digit *= base;
		n_out++;
	}

	while (n_out < numpad)
	{
		sys_write(fd, &c, 1);
		n_out++;
	}

	while (digit)
	{
		sys_write(fd, &hex[num/digit], 1);
		num %= digit;
		digit /= base;
	}

	return n_out;
}

/* very simple printf for debugging purposes */
int fd_vprintf(int fd, const char *format, va_list ap)
{
	int i=0,j=0, n_out = 0, numpad, pad;
	char c, *s;
	unsigned long num;

	while ( (c = format[i]) != '\0' )
	{
		i++;

		if (c != '%')
			continue;

		if (i-1 - j)
			n_out += sys_write(fd, &format[j], i-1 - j);

		numpad=0;
		pad = ' ';

		if (format[i] == '0')
		{
			pad = '0';
			i++;
		}

		while ( format[i] >= '0' && format[i] <= '9' )
		{
			numpad = numpad*10 + format[i] - '0';
			i++;
		}

		switch (format[i])
		{
			case 'd': case 'u':
				num = va_arg(ap, unsigned long);
				if ( (num >= LONG_MAX) && (format[i] == 'd') )
				{
					num = -num;
					sys_write(fd, "-", 1);
				}
				n_out += fd_printnum(fd, num, 10, pad, numpad);
				break;
			case 'x': case 'X':
				num = va_arg(ap, unsigned long);
				n_out += fd_printnum(fd, num, 16, pad, numpad);
				break;
			case 's':
				s = va_arg(ap, char *);
				n_out += sys_write(fd, s, strlen(s));
				break;
			case 'c':
				c = va_arg(ap, int);
				n_out += sys_write(fd, &c, 1);
				break;
			default:
				die("unimplemented format char %c\n", &format[i]);
		}

		i++;
		j=i;
	}

	if (i-j)
		n_out += sys_write(fd, &format[j], i-j);

	return n_out;
}

int fd_printf(int fd, const char *format, ...)
{
	int ret;
	va_list ap;
	va_start(ap, format);
	ret=fd_vprintf(fd, format, ap);
	va_end(ap);
	return ret;
}

char *getenve(const char *name, char **environment)
{
	int namelen = strlen(name);
	char **e = environment;

	for (e=environment; *e; e++)
		if ( (strncmp(*e, name, namelen) == 0) && ( (*e)[namelen] == '=') )
			return &(*e)[namelen+1];

	return NULL;
}

/* custom functions */

long read_at(int fd, off_t off, void *buf, size_t size)
{
	int ret = sys_lseek(fd, off, SEEK_SET);

	if (ret < 0)
		return ret;

	return sys_read(fd, buf, size);
}

char *strcat(char *dest, const char *src)
{
	char *p=dest;
	int i;
	while (*p) p++;
	for (i=0; (p[i] = src[i]); i++);
	return dest;
}

char *numcat(char *dest, long l)
{
	char *p=dest;
	unsigned long u, dec=1000000000UL;

	while (*p) p++;

	if (l < 0)
	{
		*p = '-'; p++;
		l = -l;
	}

	u = (unsigned long)l;

	while (u < dec && 1 < dec)
		dec/=10;

	for(; dec; dec/=10, p++)
		*p = (u/dec)%10 + '0';

	*p = '\0';
	return dest;
}

char *hexcat(char *dest, unsigned long ul)
{
	char *p=dest;
	while (*p) p++;

	int i;
	for (i=0; i<8; i++)
		p[i] = hex[ ( ul >> (28-(i*4)) ) & 0xf ];

	p[8] = '\0';
	return dest;
}

unsigned long numread(const char *s)
{
	unsigned long n=0UL;
	int i;

	for(i=0;; i++)
	{
		if ( (unsigned int)(s[i]-'0') < 10 )
		{
			n *= 10;
			n += s[i]-0x30;
		}
		else
			break;
	}

	return n;
}


unsigned long hexread(const char *s)
{
	unsigned long n=0UL;
	int i;

	for(i=0; i<2*(int)sizeof(unsigned long); i++)
	{
		if ( (unsigned int)(s[i]-'0') < 10 )
		{
			n <<= 4;
			n |= s[i]-0x30;
		}
		else if ( (unsigned int)((s[i]&~0x20)-'A') < 6 )
		{
			n <<= 4;
			n |= (s[i]&~0x20)-'A'+10;
		}
		else
			break;
	}

	return n;
}

unsigned long long strtohexull(char *s, char **endptr)
{
	unsigned long long n=0UL;
	int i;

	for(i=0 ;; i++)
	{
		if ( (unsigned int)(s[i]-'0') < 10 )
			n = (n<<4) | (s[i]-0x30);
		else if ( (unsigned int)((s[i]&~0x20)-'A') < 6 )
			n = (n<<4) | ((s[i]&~0x20)-'A'+10);
		else
			break;

	}
	if (endptr)
		*endptr = s+i;

	return n;
}

void copy_cmdline(char **dest, char **src)
{
	char **wipe=dest;

	for (;*wipe; wipe++)
		memset(*wipe, ' ', strlen(*wipe)+1);

	if (*src)
		strcpy(*dest, *src);

	for (src++; *src; src++)
	{
		strcat(*dest, " ");
		strcat(*dest, *src);
	}
}

/* dest is assumed to be a buffer of at least PATH_MAX+1 bytes */
int absdir(char *dest, const char *dir)
{
	int len = 0, ret = 0;

	dest[0] = '\0';

	if ( dir[0] != '/' )
	{
		ret = sys_getcwd(dest, PATH_MAX+1)+1;
		if ( ret >= 0 )
		{
			len = strlen(dest)+1;
			dest[len-1] = '/';
			dest[len] = '\0';
		}
	}

	len += strlen(dir);

	if ( ret < 0 || len > PATH_MAX )
	{
		dest[0] = '\0';
		return -1;
	}

	strcat(dest, dir);
	return 0;
}

long memscan(const char *hay, long haylen, const char *needle, long needlelen)
{
	long i, max = haylen-needlelen;
	for (i=0; i<max; i++)
		if ( memcmp(&hay[i], needle, needlelen) == 0 )
			return i;
	return -1;
}

