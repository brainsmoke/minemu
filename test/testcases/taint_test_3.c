#include <sys/mman.h>

int main(void)
{
	long *addr = mmap((void *)0x55555000, 4096,
	                   PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
	*(long *)0x55555000 = (long)addr+0xDEADBEEF;
}

