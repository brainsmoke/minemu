#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*func_t)(int,int);

int add(int a, int b)
{
	return a+b;
}

void die(void)
{
	perror(__FILE__);
	exit(1);

}

int main(int argc, char *argv[])
{
	int filedes[2];
	if ( pipe(filedes) < 0 )
		die();

	if (fork() == 0)
	{
		func_t function_pointer = add;

		if ( write(filedes[1], &function_pointer,
		     sizeof(function_pointer)) != sizeof(function_pointer) )
			die();
	}
	else
	{
		func_t tainted_pointer;

		if ( read(filedes[0], &tainted_pointer,
		     sizeof(tainted_pointer)) != sizeof(tainted_pointer)  )
			die();

		int a = 2, b = 2;
		printf("%d + %d = %d\n", a, b, tainted_pointer(a, b));
	}

	exit(0);
}
