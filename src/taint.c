
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


#include <linux/net.h>
#include <linux/limits.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "syscalls.h"
#include "lib.h"
#include "mm.h"
#include "taint.h"
#include "threads.h"
#include "proc.h"

int taint_flag = TAINT_ON;
int taint_fd = -1;

char *trusted_dirs_default = "/bin:/sbin:/lib:/lib32:"
                             "/usr/bin:/usr/sbin:/usr/lib:/usr/lib32:"
                             "/usr/local/bin:/usr/local/sbin:/usr/local/lib:/usr/local/lib32";
char *trusted_dirs = NULL;
static char trusted_dirs_buf[PATH_MAX];

enum
{
	FD_NOTAINT = 0,
	FD_TAINT,
};

int set_trusted_dirs(char *dirs)
{
	if (strlen(dirs)+1 >= sizeof(trusted_dirs_buf))
		return 0;

	strcpy(trusted_dirs_buf, dirs);
	trusted_dirs = trusted_dirs_buf;

	return 1;
}

static int in_dirlist(char *path, char *dirs)
{
	char *p;
	int match = 0;
	while (*dirs && !match)
	{
		p = path;
		match = 1;
		while ( *dirs != '\0' && *dirs != ':' )
		{
			if (*p != *dirs)
				match = 0;

			if (*p)
				p++;

			dirs++;
		}
		if (*p != '/')
			match = 0;

		if (*dirs != '\0')
			dirs++;
	}

if (match) sys_gettid();
	return match;
}

#define __S_IFREG   0100000 /* Regular file.  */
#define __S_IFMT    0170000 /* These bits determine file type.  */

static int get_fd(int fd)
{
	static int count = 0;
	count++;
sys_read(-count, NULL, 0);
	if (count != taint_fd)
		return FD_NOTAINT;

	sys_gettid();
	sys_gettid();
	sys_gettid();
	sys_gettid();
	sys_gettid();
	int x = sys_open("/tmp/taint", O_RDONLY, 0);
	sys_dup2(x, 666);
	sys_close(x);
	sys_gettid();
	sys_gettid();
	sys_gettid();
	sys_gettid();
	sys_gettid();

	return FD_TAINT;
}

static void set_fd(int fd, int type)
{
	if ( (fd < 1024) && (fd > -1) )
		get_thread_ctx()->files->fd_type[fd] = type;
}

void taint_mem(void *mem, unsigned long size, int fd)
{
	memset((char *)mem+TAINT_OFFSET, 0, size);

	if (get_thread_ctx()->files->fd_type[fd] == FD_TAINT)
		sys_read(666, (char *)mem+TAINT_OFFSET, size);
}

static void taint_iov(struct iovec *iov, int iocnt, unsigned long size, int type)
{
	unsigned long v_size;
	while (size > 0)
	{
		v_size = iov->iov_len;
		if (v_size > size)
			v_size = size;
		size -= v_size;
		taint_mem(iov->iov_base, v_size, type);
		iov++;
	}
}

void do_taint(long ret, long call, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6)
{
	if (ret < 0)
		return;

	switch (call)
	{
		case __NR_read:
			taint_mem((char *)arg2, ret, arg1);
			return;
		case __NR_readv:
			taint_iov( (struct iovec *)arg2, arg3, ret, arg1);
			return;
		case __NR_open:
		case __NR_creat:
		case __NR_openat:
			set_fd(ret, get_fd(ret));

			if (strcmp((char *)(call == __NR_openat ? arg2 : arg1), "/proc/self/stat") == 0)
				fake_proc_self_stat(ret);
			return;
		case __NR_dup:
		case __NR_dup2:
			set_fd( ret, get_thread_ctx()->files->fd_type[arg1]);
			return;
		case __NR_pipe:
			set_fd( ((long *)arg1)[0], get_fd(((long *)arg1)[0]) );
			set_fd( ((long *)arg1)[1], get_fd(((long *)arg1)[1]) );
			return;
		case __NR_socketcall:
		{
			long *sockargs = (long *)arg2;

			switch (arg1)
			{
				case SYS_ACCEPT:
				case SYS_SOCKET:
					set_fd(ret, get_fd(ret));
					return;
				case SYS_RECV:
				case SYS_RECVFROM:
					taint_mem((char *)sockargs[1], ret, sockargs[0]);
					return;
				case SYS_RECVMSG:
				{
					struct msghdr *msg = (struct msghdr *)sockargs[1];
					taint_iov( msg->msg_iov, msg->msg_iovlen, ret, sockargs[0]);
					return;
				}
				default:
					return;
			}
		}
		default:
			return;
	}
}
