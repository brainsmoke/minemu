#ifndef LIB_H
#define LIB_H

#include <unistd.h>
#include <stdarg.h>

int fd_vprintf(int fd, const char *format, va_list ap);
int fd_printf(int fd, const char *format, ...);

long read_at(int fd, off_t off, void *buf, size_t size);
char *numcat(char *dest, long l);

static inline int overlap(char *addr1, unsigned long len1, char *addr2, unsigned long len2)
{
	return ((unsigned long)addr1 < (unsigned long)addr2+len2) &&
	       ((unsigned long)addr2 < (unsigned long)addr1+len1);
}

static inline int contains(char *addr1, unsigned long len1, char *addr2)
{
	return overlap(addr1, len1, addr2, 1);
}

#endif /* LIB_H */
