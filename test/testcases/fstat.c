#include <stdio.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	struct stat s;

	if ( fstat(0, &s) < 0)
		perror("fstat: ");

	printf("st_dev = %lx, st_rdev = %lx\n", (unsigned long)s.st_dev,
	                                        (unsigned long)s.st_rdev);
}
