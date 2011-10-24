#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <asm/ldt.h>
#include <sys/mman.h>
#include <sys/syscall.h>


#include <pthread.h>
#include <signal.h>

static int killme = -1, signo;

void *hello_world(void *_)
{
	killme = syscall(__NR_gettid);
	for(;;)
	{
		usleep(100000);
		printf("Hello World!\n");
	}
}

void *kill_other(void * _)
{
	for(;;)
	{
		if (killme > 0)
			syscall(__NR_tgkill, getpid(), killme, signo);
		usleep(100000);
		printf("Bye World!\n");
	}
}

int main(int argc, char **argv)
{
	signo = SIGTERM;
	if (argc > 1)
		signo = atoi(argv[1]);
	pthread_t id1, id2;
	pthread_attr_t attr;
	
	pthread_attr_init( &attr );

	pthread_create( &id1, &attr, hello_world, (void *)0 );
	pthread_create( &id2, &attr, kill_other, (void *)1 );

	pthread_join( id1, NULL );
	pthread_join( id2, NULL );

	return 0;
}

