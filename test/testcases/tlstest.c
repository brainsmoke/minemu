#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <asm/ldt.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <pthread.h>

typedef int (*func_t)(void);

void set_fs_segment(int number)
{
	__asm__ __volatile__ ("mov %0, %%fs": "=r" (number));
}

void write_fs_var(int number)
{
	__asm__ __volatile__ ("mov %0, %%fs:0x0":: "r" (number));
}

void write_fs_func(func_t f)
{
	__asm__ __volatile__ ("mov %0, %%fs:0x4":: "r" (f));
}

int read_fs_var(void)
{
	int number;
	__asm__ __volatile__ ("mov %%fs:0x0, %0": "=r" (number));
	return number;
}

int read_fs_func(void)
{
	int number;
	__asm__ __volatile__ ("mov %%fs:0x4, %0": "=r" (number));
	return number;
}

int call_fs_func_pointer(void)
{
	printf("%d\n", read_fs_func());
	__asm__ __volatile__ (".ascii \"\\x64\\xff\\x15\\x04\\x00\\x00\\x00\"");
}

void create_fs_tls(char *base_addr, unsigned long size)
{
	static int entry_number = -1;
	struct user_desc tls =
	{
		.entry_number = entry_number,
		.base_addr = (int)base_addr,
		.limit = 1 + ( (size-1) & ~0xfff ) / 0x1000,
		.seg_32bit = 1,
		.limit_in_pages = 1,
	};

	syscall(SYS_set_thread_area, &tls);

	entry_number = tls.entry_number;

	set_fs_segment(entry_number*8 + 3);
}

void *testfs(void *whoami)
{
	char *my_tls = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	create_fs_tls(my_tls, 4096);
	write_fs_var((int)whoami);
	write_fs_func(read_fs_var);
	for(;;)
	{
		usleep(10000);
		printf("%d, %d\n", (int)whoami, call_fs_func_pointer());
	}
}

int main()
{
	pthread_t id1, id2;
	pthread_attr_t attr;
	
	pthread_attr_init( &attr );

	create_fs_tls(mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0), 4096);
	pthread_create( &id1, &attr, testfs, (void *)0 );
	pthread_create( &id2, &attr, testfs, (void *)1 );

	pthread_join( id1, NULL );
	pthread_join( id2, NULL );

	return 0;
}

