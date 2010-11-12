
#include <stdio.h>

#include "mm.h"

/* export some of our nice preprocessor defines to our linker file */

int main(void)
{
	printf(".user_end = 0x%lx;\n", USER_END);
	printf(".jit_end = 0x%lx;\n", JIT_END);
	printf("minemu_stack_bottom = 0x%lx;\n", MINEMU_STACK_BOTTOM);
}
