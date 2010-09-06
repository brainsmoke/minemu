
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

typedef void (*func_t)(void);

void sigsegv_cb(int sig)
{
	printf("NX bit is on\n");
	exit(EXIT_SUCCESS);
}

int main(void)
{
	char ret[1]; ret[0] = '\xc3';
	func_t func = (func_t)ret;
    signal(SIGSEGV, sigsegv_cb);
	func();
	printf("NX bit is off\n");
	exit(EXIT_FAILURE);
}

