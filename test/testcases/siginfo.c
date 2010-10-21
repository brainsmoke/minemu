
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

static int filedes[2];

void siginfo_cb(int sig, siginfo_t*info, void*_)
{
	write(1, "siginfo callback\n", 17);
	write(filedes[1], "syscall restarted  \n", 20);
}

void legacy_cb(int sig)
{
	write(1, "legacy callback\n", 16);
	write(filedes[1], "syscall restarted  \n", 20);
}

void wait_for_sig(void)
{
	char buf[20];
	memcpy(buf, "syscall interrupted\n", 20);
	pipe(filedes);
	alarm(1);
	read(filedes[0], buf, 20);
	write(1, buf, 20);
	close(filedes[0]);
	close(filedes[1]);
}

int main(int argc, char **argv)
{
	struct sigaction

	act = (struct sigaction) { .sa_sigaction = siginfo_cb, .sa_flags = SA_SIGINFO|SA_RESTART, };
	sigaction(SIGALRM, &act, NULL);
	wait_for_sig();

	act = (struct sigaction) { .sa_sigaction = siginfo_cb, .sa_flags = SA_SIGINFO, };
	sigaction(SIGALRM, &act, NULL);
	wait_for_sig();

	act = (struct sigaction) { .sa_handler = legacy_cb, .sa_flags = SA_RESTART, };
	sigaction(SIGALRM, &act, NULL);
	wait_for_sig();

	act = (struct sigaction) { .sa_handler = legacy_cb, .sa_flags = 0, };
	sigaction(SIGALRM, &act, NULL);
	wait_for_sig();
}
