
#include "syscalls.h"

char *data = "data";

char bss[1234];

void _start(void)
{
	char c[1];
	sys_read(0, c, 1);
	sys_exit(0);
}
