
#include "stdio.h"
#include "stdlib.h"
#include <sys/personality.h>

int main(void)
{
	printf("%08x\n", personality(0xffffffff));
	exit(EXIT_SUCCESS);
}
