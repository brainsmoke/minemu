#include <sys/mman.h>
#include <linux/personality.h>
#include <string.h>

#include "syscalls.h"
#include "error.h"
#include "load_elf.h"
#include "lib.h"
#include "error.h"
#include "mm.h"
#include "runtime.h"
#include "scratch.h"
#include "jit.h"
#include "jit_cache.h"
#include "codemap.h"
#include "sigwrap.h"

char **parse_options(char **argv)
{
	for (;;)
	{
		if ( (*argv == NULL) || (**argv != '-') )
			return argv;

		if ( strcmp(*argv, "--") == 0 )
			return argv+1;

		if ( strcmp(*argv, "-cache") == 0 )
			set_jit_cache_dir(*++argv);
		else
			die("unknown option: %s", *argv);

		argv++;
	}
}

/* not called main() to avoid warnings about extra parameters :-(  */
int minemu_main(int argc, char *argv[], char **envp, long *auxv)
{
	unsigned long pers = sys_personality(0xffffffff);
	

	if (ADDR_COMPAT_LAYOUT & ~pers)
	{
		sys_personality(ADDR_COMPAT_LAYOUT | pers);
		sys_execve("/proc/self/exe", argv, envp);
	}

	argv = parse_options(&argv[1]);

	init_minemu_mem(envp);
	sigwrap_init();
	jit_init();

	elf_prog_t prog =
	{
		.filename = argv[0],
		.argv = &argv[1],
		.envp = envp,
		.auxv = auxv,
		.task_size = USER_END,
		.stack_size = USER_STACK_SIZE,
	};

	int ret = load_elf(&prog);
	if (ret < 0)
		die("load_elf: %d", ret);

	jit(prog.entry);

	code_map_t *map = find_code_map(prog.entry);

	char *user_to_jit[map->len];
	char *jit_to_user[map->jit_len];
	memset(user_to_jit, 0, map->len*sizeof(long));
	memset(jit_to_user, 0, map->jit_len*sizeof(long));
	int i, j=-1, jnext=1000000000, jlast=0;
	char *rev_addr=NULL, *rev_last=NULL;
	char *op_start;
	long op_len;

	for (i=0; i<map->len; i++)
	{
		user_to_jit[i] = jit_lookup_addr(&map->addr[i]);
		if ( user_to_jit[i] )
		{
			j = (long)user_to_jit[i]-(long)map->jit_addr;
			jit_to_user[j] = &map->addr[i];
			if (j < jnext)
				jnext = j;
			if (j > jlast)
				jlast = j;
		}
	}

	for (j=jnext; j<jlast; j++)
	{
		rev_addr = jit_rev_lookup_addr(&map->jit_addr[j], &op_start, &op_len);

		if (jit_to_user[j])
		{
			if (jit_to_user[j] != rev_addr)
				debug("reverse mapping bad %x expected %x j=%x", rev_addr, jit_to_user[j], j);

			else if (&map->jit_addr[j] != op_start)
				die("translation starts before expected address %x %x", rev_addr, op_start);
		}
		else
		{
			if (rev_addr && rev_addr != rev_last)
				die("start address changes in the middle of opcode %x to %x", rev_last, rev_addr);
		}

		if (rev_addr && user_to_jit[(long)rev_addr-(long)map->addr] != op_start)
			die("reverse mapping does not match %x to %x",
			    user_to_jit[(long)rev_addr-(long)map->addr], op_start);

		rev_last = rev_addr;
	}

	sys_exit(0);
	return 0;
}
