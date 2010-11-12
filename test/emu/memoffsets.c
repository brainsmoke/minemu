#include <stdio.h>

#include "mm.h"

void print_addr(const char *name, unsigned long long l)
{
	printf("%16s:  0x%08llx%12llu%6lluMiB\n", name, l, l, l/1048576);
}

void print_pages(const char *name, unsigned long long l)
{
	printf("%16s:    0x%06llx%12llu\n", name, l, l);
}

int main(void)
{
	printf("\n");

	print_addr("USER_START", USER_START);
	print_addr("USER_END", USER_END);
	print_addr("JIT_START", JIT_START);
	print_addr("JIT_END", JIT_END);
	print_addr("TAINT_START", TAINT_START);
	print_addr("TAINT_END", TAINT_END);
	print_addr("MINEMU_START", MINEMU_START);
	print_addr("HIGH_USER_ADDR", HIGH_USER_ADDR);
	printf("\n");

	print_addr("TAINT_MOVE", TAINT_MOVE);
	printf("\n");

	print_addr("USER_SIZE", USER_SIZE);
	print_addr("JIT_SIZE", JIT_SIZE);
	print_addr("TAINT_SIZE", TAINT_SIZE);
	print_addr("MINEMU_SIZE", MINEMU_SIZE);
	printf("\n");

	print_pages("USER_PAGES", USER_PAGES);
	print_pages("JIT_PAGES", JIT_PAGES);
	print_pages("TAINT_PAGES", TAINT_PAGES);
	print_pages("MINEMU_PAGES", MINEMU_PAGES);
	printf("\n");

	print_pages("HIGH_USER_PAGE", HIGH_USER_PAGE);
	print_pages("HIGH_PAGE", HIGH_PAGE);
	printf("\n");
}
