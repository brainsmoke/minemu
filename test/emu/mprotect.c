
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
