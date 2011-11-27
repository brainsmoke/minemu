
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int filedes[2];
	char buf[2084];
	pipe(filedes);
	int i;
	for (i=0; i<sizeof(buf); i++)
		write(filedes[1], "X", 1);
	read(filedes[0], buf, sizeof(buf));
}
