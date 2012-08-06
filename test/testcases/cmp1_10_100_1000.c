#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd=open(argv[1], O_RDONLY);
	char buf[2000];
	char bla[2000];
	read(fd, buf, 2000);
	int c = buf[1];
	if (buf[10] == 6)
		c = buf[1000];
	if (buf[100] == buf[1000])
		c = buf[100];
	if (c == buf[1])
		return 1;
}
