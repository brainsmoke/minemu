#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <asm/ldt.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <pthread.h>

void *hello_world(void *_)
{
	for(;;)
	{
		usleep(100000);
		printf("Hello World!\n");
	}
}

void *segv(void * _)
{
	_ = *(void **)0x0;
}

int main()
{
	pthread_t id1, id2;
	pthread_attr_t attr;
	
	pthread_attr_init( &attr );

	pthread_create( &id1, &attr, hello_world, (void *)0 );
	pthread_create( &id2, &attr, segv, (void *)1 );

	pthread_join( id1, NULL );
	pthread_join( id2, NULL );

	return 0;
}

