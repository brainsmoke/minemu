#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <limits.h>

#include "syscalls.h"
#include "error.h"

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
	for (i=0; dest[i]=src[i]; i++);
	return dest;
}

size_t strlen(const char *s)
{
	size_t i;
	for (i=0; s[i]; i++);
	return i;
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
	int i=0, n_out = 0, numpad, pad;
	char c, *s;
	unsigned long num;

	while ( (c = format[i]) != '\0' )
	{
		i++;

		if (c != '%')
		{
			n_out += sys_write(fd, &c, 1);
			continue;
		}

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
				if ( (num > LONG_MAX) && (c == 'd') )
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
	}

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

/* custom functions */

long read_at(int fd, off_t off, void *buf, size_t size)
{
	int ret = sys_lseek(fd, off, SEEK_SET);

	if (ret < 0)
		return ret;

	return sys_read(fd, buf, size);
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

/*
static void swap(void *a, void *b, size_t size)
{
	if (a==b)
		return;

	char tmp[size];
	memcpy(tmp, a, size);
	memcpy(a, b, size);
	memcpy(b, tmp, size);
}

#define E(n) &((char*)base)[(n)*size]

void qsort(void *base, size_t n, size_t size,
           int(*cmp)(const void *, const void *))
{
	if (n < 2)
		return;

	swap(E(n/2), E(n-1), size);

	unsigned long i, part=0;

	for (i=0; i<n-1; i++)
		if (cmp(E(i), E(n-1)) <= 0)
		{
			swap(E(i), E(part), size);
			part += 1;
		}

	swap(E(n-1), E(part), size);

	qsort(E(0)     ,   part  , size, cmp);
	qsort(E(part+1), n-part-1, size, cmp);
}
*/

long memscan(const char *hay, long haylen, const char *needle, long needlelen)
{
	long i, max = haylen-needlelen;
	for (i=0; i<max; i++)
		if ( memcmp(&hay[i], needle, needlelen) == 0 )
			return i;
	return -1;
}

