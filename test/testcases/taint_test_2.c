#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*func_t)(void);

void mal(void)
{
	printf("MAL!\n");
	exit(1);
}

void die(void)
{
	perror(__FILE__);
	exit(1);
}

void do_something(func_t function_pointer)
{
	int filedes[2];
	if ( pipe(filedes) < 0 )
		die();

	if (fork() == 0)
	{
		if ( write(filedes[1], &function_pointer,
		     sizeof(function_pointer)) != sizeof(function_pointer) )
			die();
	}
	else
	{
		if ( read(filedes[0], &function_pointer-1,
		     sizeof(function_pointer)) != sizeof(function_pointer)  )
			die();
	}


}

int main(int argc, char *argv[])
{
	do_something(mal);
	exit(0);
}
