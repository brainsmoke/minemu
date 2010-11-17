
#include <fcntl.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <linux/unistd.h>

#include <string.h>

#include "errors.h"
#include "trace.h"
#include "util.h"
#include "serialize.h"
#include "syscall_info.h"
#include "syscall_copy.h"
#include "call_data.h"
#include "debug.h"

static void copy_iov_from_uspace(out_stream_t *out, trace_t *t, uspace_buf_t *b)
{
	ssize_t total_size = get_call_data(t, b->len);
	size_t size;
	void *raddr;
	call_data_t *d = call_data(t);
	unsigned int i;

	for (i=0; i<d->iovcnt && total_size > 0; i++)
	{
		raddr = d->iovec[i].iov_base;
		size = d->iovec[i].iov_len;
		if ( (unsigned long)total_size < size )
			size = total_size;

		total_size -= size;

		char buf[size];
		size = memload_pipe(t, buf, raddr, size, signal_queue(t));
		write_uspace_copy(out, IOV_PTR(i), buf, size);
	}
}

static void copy_from_uspace(out_stream_t *out, trace_t *t, uspace_buf_t *b)
{
	ssize_t size;
	void *raddr;

	if (b->type == IOVEC_COPY)
	{
		copy_iov_from_uspace(out, t, b);
		return;
	}

	raddr = (void*)get_call_data(t, b->ptr);

	if ( ( raddr == NULL ) || 
	     ( ((long)raddr < 0) && ((long)raddr > -512)) )
		return;

	switch (b->type)
	{
		case FIXED_SIZE:
		case NULL_TERMINATED: /* max_size */
			size = b->size;
			break;
		case VAR_SIZE:
			size = get_call_data(t, b->len);
			break;
		case FD_SET_SIZE:
			size = ( (get_call_data(t, b->len)+sizeof(long)*8-1) /
			         (sizeof(long)*8) ) * (sizeof(long));
			break;
		case STRUCT_ARRAY:
			size = get_call_data(t, b->len)*b->size;
			break;
		default:
			size = -1;
			fatal_error("unsupported uspace copy");
	}

	if (size >= 0)
	{
		char buf[size];
		if ( b->type == NULL_TERMINATED )
			size = memloadstr(t->pid, buf, raddr, size)+1;
		else
//			memload(t->pid, buf, raddr, size);
			size = memload_pipe(t, buf, raddr, size, signal_queue(t));

		write_uspace_copy(out, b->ptr, buf, size);
	}
}

static void copy_from_uspace_list(out_stream_t *out, trace_t *t,
                                  uspace_buf_t *b)
{
	for (; b->type != LIST_END; b++)
		copy_from_uspace(out, t, b);
}

void serialize_post_call_data(out_stream_t *out, trace_t *t)
{
	syscall_info_t *info = syscall_info(t);

	if (info->copy)
		copy_from_uspace_list(out, t, info->copy);

	if (t->syscall == __NR_fcntl64 && get_arg(t, 2) == F_GETLK)
		copy_from_uspace_list(out, t, flock_copy);

	if (t->syscall == __NR_fcntl && get_arg(t, 2) == F_GETLK64)
		copy_from_uspace_list(out, t, flock64_copy);

	/*
	if (t->syscall == __NR_ioctl)
		...
	*/
}

void unserialize_post_call_data(in_stream_t *in, trace_t *t)
{
	char *buf;
	long argno;
	size_t len;

	while ( peek_type(in) == USPACE_COPY )
	{
		buf = read_uspace_copy(in, &argno, &len);
		memstore_pipe(t, buf, (void*)get_call_data(t, argno), len, NULL);
//		memstore(t->pid, buf, (void*)get_call_data(t, argno), len);
	}
}

void compare_user_data(in_stream_t *in, trace_t *t)
{
	char *buf;
	long argno;
	size_t len;

	while ( peek_type(in) == USPACE_COPY )
	{
		buf = read_uspace_copy(in, &argno, &len);
		char buf2[len];
		memload_pipe(t, buf2, (void*)get_call_data(t, argno), len, NULL);
		if ( bcmp(buf, buf2, len) )
			printhex_diff(buf, len, buf2, len, 1);
	}
}
