#include <linux/personality.h>
#include <unistd.h>
int personality(unsigned long persona);

int main(int argc, char **argv)
{
	personality(personality(0xffffffff)|ADDR_COMPAT_LAYOUT);
	execvp(argv[1], &argv[1]);
}
