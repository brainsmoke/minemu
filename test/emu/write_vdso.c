#include <linux/personality.h>
#include <unistd.h>
unsigned int personality(unsigned long persona);

int main(int argc, char **argv)
{
	unsigned int p = personality(0xffffffff);
	if (p&ADDR_COMPAT_LAYOUT)
	{
		write(1, (char *)0x4001c000, 0x1000);
	}
	else
	{
		personality(p|ADDR_COMPAT_LAYOUT);
		execvp("/proc/self/exe", argv);
	}
}
