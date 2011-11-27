#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <error.h>
#include <fcntl.h>

void *mmap_file(char *name, int mode)
{
	void *addr;
	int fd = open(name, mode);

	struct stat s;
	if ( fstat(fd, &s) < 0)
		perror("fstat: ");

	addr = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return addr;
}

int main(int argc, char *argv[])
{
	mmap_file(argv[1], O_RDONLY);

	int fd = open("/proc/self/maps", O_RDONLY);
	char buf[2048];
	long n_read;

	while ( (n_read = read(fd, buf, sizeof(buf))) > 0 )
		write(1, buf, n_read);
}
