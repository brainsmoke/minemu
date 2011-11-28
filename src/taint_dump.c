
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

#include <linux/limits.h>

#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "mm.h"
#include "error.h"
#include "sigwrap.h"
#include "syscalls.h"

#include "taint_dump.h"
#include "hexdump.h"
#include "threads.h"
#include "taint.h"
#include "proc.h"

int dump_on_exit = 0;
int dump_all = 0;

const char *regs_desc[] =
{
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
};


void hexdump_taint(int fd, const void *data, ssize_t len,
                           const unsigned char *taint, int offset, int ascii,
                           const char *description[])
{
	const char *colors[256];
	int i;
	char *red    = "\033[1;31m",
	     *blue   = "\033[1;36m",
	     *green  = "\033[1;32m",
	     *yellow = "\033[1;33m",
	     *white  = "\033[1;37m",
	     *dark   = "\033[1;30m",
	     *grey   = "\033[0;37m";
	
	for(i=0; i<256; i++)
		colors[i] = dark;

	colors[TAINT_CLEAR]          = grey;
	colors[TAINT_SOCKADDR]       = blue;
	colors[TAINT_ENV]            = green;
	colors[TAINT_FILE]           = yellow;
	colors[TAINT_ENV|TAINT_FILE] = white;
	colors[TAINT_SOCKET]         = red;

	hexdump(fd, data, len, offset, ascii, description, taint, colors);
}

void dump_map(int fd, char *addr, unsigned long len)
{
	long *laddr;
	unsigned long i,j, last=0xFFFFFFFF;
	int t;

	i=0;

	/* trim the stack a bit */
	if ( dump_all && (long)addr == (long)(USER_END-USER_STACK_SIZE) )
	{
		for (; i<len; i++)
			if ( addr[i] || addr[i+TAINT_OFFSET] )
				break;

		for (; len>0; len--)
			if ( addr[len-1] || addr[len-1+TAINT_OFFSET] )
				break;

		i = PAGE_BASE(i);
		len = PAGE_NEXT(len);
	}

	for (; i<len; i+=PG_SIZE)
	{
		t = dump_all;

		if (!t)
		{
			laddr = (long *)&addr[i+TAINT_OFFSET];
			for (j=0; j<PG_SIZE/sizeof(long); j++)
				if (laddr[j])
					t=1;
		}

		if (t)
		{
			if (last == 0xFFFFFFFF)
				fd_printf(fd, "in map: %x (size %u)\n", addr, len);
			else if (i != last+PG_SIZE)
				fd_printf(fd, "...\n");

			hexdump_taint(fd, &addr[i], PG_SIZE,
			              (unsigned char *)&addr[i+TAINT_OFFSET], 1, 1, NULL);
			last = i;
		}
	}
}

static char taint_dump_dir_buf[PATH_MAX+1] = { 0, };

static char *taint_dump_dir = NULL;

void set_taint_dump_dir(const char *dir)
{
	if ( absdir(taint_dump_dir_buf, dir) == 0 )
		taint_dump_dir = taint_dump_dir_buf;
	else
		taint_dump_dir = NULL;
}

char *get_taint_dump_dir(void)
{
	return taint_dump_dir;
}

static char *get_taint_dump_filename(char *buf)
{
	buf[0] = '\x0';
	strcat(buf, taint_dump_dir);
	strcat(buf, "/taint_hexdump_");
	numcat(buf, sys_gettid());
	strcat(buf, ".dump");
	return buf;
}

void do_taint_dump(long *regs)
{
	if ( taint_dump_dir == NULL )
		return;

	char name[PATH_MAX+1+64];
	get_taint_dump_filename(name);
	int fd = sys_open(name, O_RDWR|O_CREAT, 0600);

	fd_printf(fd, "jump address:\n");

	hexdump_taint(fd, &get_thread_ctx()->user_eip, 4,
	              (unsigned char *)&get_thread_ctx()->ijmp_taint, 0,0, NULL);

	fd_printf(fd, "registers:\n");

	unsigned char regs_taint[32];
	get_xmm6(&regs_taint[0]);
	get_xmm7(&regs_taint[16]);
	hexdump_taint(fd, regs, 32, regs_taint, 0, 1, regs_desc);

	map_file_t f;
	map_entry_t e;
	open_maps(&f);
	while (read_map(&f, &e) && e.addr < USER_END)
		if (e.prot)
			dump_map(fd, (char *)e.addr, e.len);
	close_maps(&f);

	sys_close(fd);
}

