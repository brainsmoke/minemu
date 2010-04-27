
#include <unistd.h>

int main(void)
{
	char c[1048576];
	read(0, c, 1);
	main();
}
