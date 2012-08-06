#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd=open(argv[1], O_RDONLY);
	char buf[2000];
	char bla[2000];
	read(fd, buf, 2000);
	bla[1] = buf[1];
	bla[10] = buf[10];
	bla[100] = buf[100];
	bla[1000] = buf[1000];
}
