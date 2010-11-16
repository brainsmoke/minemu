
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	for (; *argv ;argv++)
		printf("%s\n", *argv);

	exit(EXIT_SUCCESS);
}
