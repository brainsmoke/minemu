
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>

/* bogus loop
 *
 */
void do_loop(long long count)
{
	long long i;
	int a = 1, b = 1, c;
	for (i=0; i<count; i++)
	{
		c = b + a;
		a = b;
		b = c;
	}
}

void sigchld_cb(int sig)
{
	write(1, "sigchld_cb\n", 11);
	do_loop(100000000);
	write(1, "sigchld_cb\n", 11);
}

void sigalrm_cb(int sig)
{
	write(1, "SIGALRM\n", 8);
}

int main(int argc, char **argv)
{
	alarm(1);
	signal(SIGALRM, sigalrm_cb);
	signal(SIGCHLD, sigchld_cb);
	if (!fork())
		exit(0);

	do_loop(10000000);
	wait(NULL);
}
