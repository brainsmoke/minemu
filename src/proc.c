
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 * Copyright 2011 Vrije Universiteit Amsterdam
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#include "syscalls.h"
#include "error.h"
#include "lib.h"
#include "proc.h"
#include "mm.h"
#include "threads.h"

/* fake /proc/self/stat since libgc depends on it
 * we need to patch the stack bottom value
 */
#define STACK_SKIP (27)
void fake_proc_self_stat(long fd)
{
	char buf[2048], *p=buf;
	long filedes[2], n_read, i;

	n_read = sys_read(fd, buf, sizeof(buf));
	if (n_read < 0)
		return;

	if (sys_pipe(filedes) < 0)
		die("fake_proc_self_stat(): sys_pipe()");

	if (sys_dup2(filedes[0], fd) < 0)
		die("fake_proc_self_stat(): sys_dup2()");

	sys_close(filedes[0]);

	for (i=0; i<STACK_SKIP; i++)
		p = strchr(p, ' ')+1;

	sys_write(filedes[1], buf, p-buf);
	p = strchr(p, ' ')+1;
	p = strchr(p, ' ')+1;
	p = strchr(p, ' ');

	thread_ctx_t *ctx = get_thread_ctx();

	buf[0]='\0';
	numcat(buf, stack_bottom);
	strcat(buf, " ");
	numcat(buf, ctx->user_esp);
	strcat(buf, " ");
	numcat(buf, ctx->user_eip);
	sys_write(filedes[1], buf, strlen(buf));
	sys_write(filedes[1], p, buf+n_read-p);

	sys_close(filedes[1]);
}

/* simple /proc/self/maps parser: */

static int map_eof(map_file_t *f)
{
	if (f->i == f->n_read)
	{
		f->n_read = sys_read(f->fd, f->buf, sizeof(f->buf));
		f->i = (f->n_read >= 0) ? 0 : -1;
	}
	return f->i == -1;
}

static int map_getc(map_file_t *f)
{
	return (!map_eof(f)) ? f->buf[f->i++] : -1;
}

int open_maps(map_file_t *f)
{
	*f = (map_file_t) { .fd = sys_open("/proc/self/maps", O_RDONLY, 0), };

	if (f->fd < 0)
		die("could not open /proc/self/maps");

	return f->fd;
}

int close_maps(map_file_t *f)
{
	return sys_close(f->fd);
}

static unsigned long read_addr(map_file_t *f)
{
	unsigned long addr = 0;
	int i,c;
	for(i=0; i<8; i++)
	{
		addr *= 16;
		c = map_getc(f);
		if ( (c|0x20) >= 'a' && (c|0x20) <= 'f' )
			addr += 9;
		else if ( c < '0' || c > '9' )
			die("not an address");

		addr += c & 0xf;
	}
	return addr;
}

int read_map(map_file_t *f, map_entry_t *e)
{
	int c;
	if (map_eof(f))
		return 0;

	e->addr = read_addr(f);
	map_getc(f); /* '-' */
	e->len = read_addr(f) - e->addr;
	map_getc(f); /* ' ' */
	e->prot = 0;
	if (map_getc(f) == 'r') e->prot |= PROT_READ;
	if (map_getc(f) == 'w') e->prot |= PROT_WRITE;
	if (map_getc(f) == 'x') e->prot |= PROT_EXEC;

	while ( (c=map_getc(f) != '\n') && (c > 0) );

	return 1;
}

