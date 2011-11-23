#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>

int main(void)
{
	struct rlimit rlim;
	getrlimit(RLIMIT_STACK, &rlim);
	printf("cur: %ld max: %ld\n", rlim.rlim_cur, rlim.rlim_max);

	return 0;
}
