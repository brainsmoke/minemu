#ifndef LIB_H
#define LIB_H

#include <unistd.h>
#include <stdarg.h>

#undef isprint
int isprint(int c);

int fd_vprintf(int fd, const char *format, va_list ap);
int fd_printf(int fd, const char *format, ...);

long read_at(int fd, off_t off, void *buf, size_t size);
char *numcat(char *dest, long l);

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
