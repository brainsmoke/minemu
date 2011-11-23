#include <linux/limits.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void)
{
	char path[PATH_MAX+1];
	errno=0;
	long ret = readlink("/proc/self/fd/0", path, PATH_MAX);
	path[ret >= 0 ? ret : 0] = '\0';

	printf("readlink(): %s : %s\n", path, strerror(ret));

	return 0;
}
