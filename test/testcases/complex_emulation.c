
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void sigalrm_cb(int sig)
{
	write(1, "SIGALRM\n", 8);
	exit(0);
}

int some_call()
{
	return 3;
}

int main(int argc, char **argv)
{
	int (*call)(void) = some_call;
	alarm(1);
	signal(SIGALRM, sigalrm_cb);
	for ( ;; ) call();
}
