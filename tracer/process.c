#define _LARGEFILE64_SOURCE 1
#define _GNU_SOURCE 1

#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>

#include <sys/wait.h>

#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/prctl.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "process.h"
#include "errors.h"

/* Get/set the process' ability to use the timestamp counter instruction.
 * This is be available in linux 2.6.26 on x86
 */
#ifndef PR_GET_TSC
#define PR_GET_TSC 25
#define PR_SET_TSC 26
# define PR_TSC_ENABLE      1   /* allow the use of the timestamp counter */
# define PR_TSC_SIGSEGV     2   /* throw a SIGSEGV instead of reading the TSC */
#endif

/* why doesn't POSIX define this?
 * subtle behaviour quirk: the PATH used is the path of the new environment
 */
static int execvpe(char *filename, char *argv[], char *envp[])
{
	char **tmp = environ;
	environ = envp;
	execvp(filename, argv);
	environ = tmp;
	return -1;
}

pid_t run_traceable(char *path, char *args[], int no_randomize, int notsc)
{
	return run_traceable_env(path, args, environ, no_randomize, notsc);
}

pid_t run_traceable_env(char *path, char *args[], char *envp[],
                        int no_randomize, int notsc)
{
	pid_t pid = fork();

	if (pid > 0)
		return pid;

	if (pid < 0)
		fatal_error("fork: %s", strerror(errno));

	/* turn off address space randomization */
	if ( no_randomize && ( personality(personality(0xffffffff) |
	                                   ADDR_NO_RANDOMIZE) == -1) )
	{
		perror("personality");
		exit(EXIT_FAILURE);
	}

	if ( notsc && prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0) < 0 )
		fprintf(stderr, "warning, disabling RDTSC failed\n");

	if ( ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1 )
		perror("ptrace");
	else
	{
		execvpe(args[0], args, envp);
		perror("exec");
	}
	exit(EXIT_FAILURE);
}

static char *get_link(const char *path)
{
	char buf[4096];
	ssize_t len = readlink(path, buf, 4096);

	if (len < 0)
		fatal_error("readlink: %s", strerror(errno));

	char *s = try_malloc(len+1);

	memcpy(s, buf, len);
	s[len]='\0';

	return s;
}

static char *get_proc_file(pid_t pid, const char *s)
{
	int maxlen = 32+strlen(s);
	char name[maxlen];
	int len = snprintf(name, maxlen, "/proc/%u/%s", pid, s);

	if ( (len >= maxlen) || (len < 0) )
		fatal_error("%s: snprintf failed where it shouldn't", __FUNCTION__);

	return get_link(name);
}

char *get_proc_exe(pid_t pid)
{
	return get_proc_file(pid, "exe");
}

char *get_proc_cwd(pid_t pid)
{
	return get_proc_file(pid, "cwd");
}

char *get_proc_fd(pid_t pid, int fd)
{
	char pidname[64];
	int len = snprintf(pidname, 64, "/proc/%u/%d", pid, fd);

	if ( (len >= 64) || (len < 0) )
		fatal_error("%s: snprintf failed where it shouldn't", __FUNCTION__);

	return get_link(pidname);
}

