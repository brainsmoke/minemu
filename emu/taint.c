
#include <linux/net.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "syscalls.h"
#include "lib.h"
#include "mm.h"

extern char fd_type[1024];

enum
{
	FD_UNKNOWN = 0,
	FD_SOCKET,
	FD_NO_SOCKET,
};

int is_socket(int fd)
{
	if ( (fd > 1023) || (fd < 0) )
		return 0;

	if ( fd_type[fd] == FD_UNKNOWN )
	{
		long len=0;
		char buf[1];
		long args[] = { fd, (long)buf, (long)&len };
		long ret = syscall2(__NR_socketcall, SYS_GETSOCKNAME, (long)args);
		fd_type[fd] = ( (ret == -EBADF) || (ret == -ENOTSOCK) ) ? FD_NO_SOCKET : FD_SOCKET;
	}

	return fd_type[fd] == FD_SOCKET;
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
	while (size < 0)
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
		case __NR_dup:
		case __NR_dup2:
		case __NR_openat:
			set_fd(ret, FD_NO_SOCKET);
			return;
		case __NR_pipe:
			set_fd( ((long *)arg1)[0], FD_NO_SOCKET);
			set_fd( ((long *)arg1)[1], FD_NO_SOCKET);
			return;
		case __NR_socketcall:
		{
			long *sockargs = (long *)arg2;

			switch (arg1)
			{
				case SYS_SOCKET:
				case SYS_ACCEPT:
					set_fd(sockargs[0], FD_SOCKET);
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
