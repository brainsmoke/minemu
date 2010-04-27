
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "lib.h"
#include "error.h"

#include "syscalls.h"

void debug(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fd_vprintf(2, fmt, ap);
	va_end(ap);
	fd_printf(2, "\n");
}

void die(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fd_vprintf(2, fmt, ap);
	va_end(ap);
	fd_printf(2, "\n");
	abort();
}
