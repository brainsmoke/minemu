
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
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
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "lib.h"
#include "mm.h"
#include "taint.h"

extern char fd_type[1024];

int taint_flag = TAINT_ON;

enum
{
	FD_UNKNOWN = 0,
	FD_SOCKET,
	FD_NO_SOCKET,
};


#define _LARGEFILE64_SOURCE 1
#include <asm/stat.h>
#define __S_IFREG   0100000 /* Regular file.  */
#define __S_IFMT    0170000 /* These bits determine file type.  */

int is_socket(int fd)
{
	if ( (fd > 1023) || (fd < 0) )
		return 0;

	if ( fd_type[fd] == FD_UNKNOWN )
	{
		struct stat64 s;
		if ( ( sys_fstat64(fd, &s) < 0 ) || (s.st_mode & __S_IFMT) != __S_IFREG )
			fd_type[fd] = FD_SOCKET;
		else
			fd_type[fd] = FD_NO_SOCKET;
	}

	return fd_type[fd] == FD_SOCKET;;
}

void set_fd(int fd, int type)
{
	if ( (fd < 1024) && (fd > -1) )
		fd_type[fd] = type;
}

void taint_mem(void *mem, unsigned long size, int type)
{
	memset((char *)mem+TAINT_OFFSET, type, size);
}

void taint_iov(struct iovec *iov, int iocnt, unsigned long size, int type)
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
			taint_mem((char *)arg2, ret, is_socket(arg1) ? 0xff : 0x00);
			return;
		case __NR_readv:
			taint_iov( (struct iovec *)arg2, arg3, ret, is_socket(arg1) ? 0xff : 0x00);
			return;
		case __NR_open:
		case __NR_creat:
		case __NR_openat:
			set_fd(ret, FD_NO_SOCKET);
			return;
		case __NR_dup:
		case __NR_dup2:
			set_fd( ret, fd_type[arg1]);
			return;
		case __NR_pipe:
			set_fd( ((long *)arg1)[0], FD_SOCKET);
			set_fd( ((long *)arg1)[1], FD_SOCKET);
			return;
		case __NR_socketcall:
		{
			long *sockargs = (long *)arg2;

			switch (arg1)
			{
				case SYS_GETPEERNAME:
					if ( (ret >= 0) && sockargs[1] && sockargs[2])
						taint_mem((char *)sockargs[1], *(long *)sockargs[2], 0x01);
					return;
				case SYS_ACCEPT:
					if ( (ret >= 0) && sockargs[1] && sockargs[2])
						taint_mem((char *)sockargs[1], *(long *)sockargs[2], 0x01);
				case SYS_SOCKET:
					set_fd(ret, FD_SOCKET);
					return;
				case SYS_RECV:
				case SYS_RECVFROM:
					taint_mem((char *)sockargs[1], ret, 0xff);
					return;
				case SYS_RECVMSG:
				{
					struct msghdr *msg = (struct msghdr *)sockargs[1];
					taint_iov( msg->msg_iov, msg->msg_iovlen, ret, 0xff );
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
