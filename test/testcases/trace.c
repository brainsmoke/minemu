
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

void trace(pid_t pid)
{
	int status, sig;

	while (ptrace(PTRACE_SETOPTIONS, pid, -1,
	              PTRACE_O_TRACEFORK |
	              PTRACE_O_TRACEVFORK |
	              PTRACE_O_TRACECLONE |
	              PTRACE_O_TRACESYSGOOD) != 0);

	pid = waitpid(-1, &status, __WALL);
	status|=0x80<<8;
	while (pid > -1)
	{
		sig = (status>>8) & 0xff;
		if (sig & 0x80)
			sig = 0;

//		if (sig)
//			signal_translate_pc(pid);
//		else		
			ptrace(PTRACE_CONT, pid, 0, sig);

		pid = waitpid(-1, &status, __WALL);
	}
}

void signal_wrapper(void)
{
	pid_t pid = fork();

	if (pid > 0)
	{
		trace(pid);
		exit(EXIT_SUCCESS);
	}
	else if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}
	if ( ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1 )
	{
		exit(EXIT_FAILURE);
	}	
}

int main(int argc, char *argv[])
{
	signal_wrapper();

	execvp(argv[1], &argv[1]);
	perror("exec");
	exit(EXIT_FAILURE);
}

