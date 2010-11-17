
#include <stdlib.h>

#include "trace.h"
#include "util.h"
#include "dataset.h"
#include "errors.h"
#include "syscall_info.h"
#include "call_data.h"

/*  */

static int CALL_DATA = -1;

call_data_t *call_data(trace_t *t)
{
	return (call_data_t *)get_data((dataset_t*)&t->data, CALL_DATA);
}

/* Make accessing call data uniform
 *
 */
long get_call_data(trace_t *t, unsigned long which)
{
	if ( which >= ARG_BASE && which < ARG_BASE+8 )
		return get_arg(t, which-ARG_BASE);

	call_data_t *d = call_data(t);

	if ( which >= SOCK_ARG_BASE && which < SOCK_ARG_BASE+8 )
		return d->sock_arg[which-SOCK_ARG_BASE];

	if ( which >= IOV_BASE && which < IOV_BASE+d->iovcnt )
		return (long)d->iovec[which-IOV_BASE].iov_base;

	switch ( which )
	{
		case RETURN_VALUE:
			if (t->state == PRE_CALL)
				fatal_error("cannot use retval before the "
				            "syscall has finished");
			return get_result(t);
		case PRE_LEN:
			return d->pre_len;
		case POST_LEN:
			return d->post_len;
		case MIN_LEN:
			return d->pre_len<d->post_len ? d->pre_len:d->post_len;

		case PRE_MSG_NAME:
			return (long)d->pre_msg.msg_name;
		case PRE_MSG_NAME_LEN:
			return d->pre_msg.msg_namelen;
		case PRE_MSG_IOV:
			return (long)d->pre_msg.msg_iov;
		case PRE_MSG_IOVLEN:
			return d->pre_msg.msg_iovlen;
		case PRE_MSG_CONTROL:
			return (long)d->pre_msg.msg_control;
		case PRE_MSG_CONTROLLEN:
			return d->pre_msg.msg_controllen;
		case PRE_MSG_FLAGS:
			return d->pre_msg.msg_flags;

		case POST_MSG_NAME:
			return (long)d->post_msg.msg_name;
		case POST_MSG_NAME_LEN:
			return d->post_msg.msg_namelen;
		case POST_MSG_IOV:
			return (long)d->post_msg.msg_iov;
		case POST_MSG_IOVLEN:
			return d->post_msg.msg_iovlen;
		case POST_MSG_CONTROL:
			return (long)d->post_msg.msg_control;
		case POST_MSG_CONTROLLEN:
			return d->post_msg.msg_controllen;
		case POST_MSG_FLAGS:
			return d->post_msg.msg_flags;
		default:
			fatal_error("unknown call data");
			return -1;
	}

}

void load_pre_call_data(trace_t *t)
{
	syscall_info_t *info = syscall_info(t);
	call_data_t *d = call_data(t);

	if (info->sock_argc)
		memload(t->pid, &d->sock_arg[0], (void *)get_arg(t, 1),
		        sizeof(long)*info->sock_argc);

	if (info->msg_ptr)
		memload_pipe(t, &d->pre_msg,
		             ((long *)get_call_data(t, info->msg_ptr)),
		             sizeof(struct msghdr), signal_queue(t));

	if (info->len_ptr)
		memload(t->pid, &d->pre_len,
		        ((long *)get_call_data(t, info->len_ptr)),
		        sizeof(long));

	if (info->iov_ptr)
	{
		d->iovcnt = get_call_data(t, info->iov_cnt);
		if ( d->iovcnt > 0 && d->iovcnt <= UIO_MAXIOV )
			memload_pipe(t, &d->iovec,
			             ((long *)get_call_data(t, info->iov_ptr)),
			             sizeof(struct iovec)*d->iovcnt, signal_queue(t));
	}
}

void load_post_call_data(trace_t *t)
{
	syscall_info_t *info = syscall_info(t);
	call_data_t *d = call_data(t);

	if (info->msg_ptr)
		memload_pipe(t, &d->post_msg,
		             ((long *)get_call_data(t, info->msg_ptr)),
		             sizeof(struct msghdr), signal_queue(t));

	if (info->len_ptr)
		memload(t->pid, &d->post_len,
		        ((long *)get_call_data(t, info->len_ptr)),
		        sizeof(long));
}

void save_post_call_data(trace_t *t)
{
	syscall_info_t *info = syscall_info(t);
	call_data_t *d = call_data(t);

	if (info->msg_ptr)
		memstore_pipe(t, &d->post_msg,
		              ((long *)get_call_data(t, info->msg_ptr)),
		              sizeof(struct msghdr), signal_queue(t));

	if (info->len_ptr)
		memstore(t->pid, &d->post_len,
		         ((long *)get_call_data(t, info->len_ptr)),
		         sizeof(long));
}

call_data_t *add_call_data(trace_t *t)
{
	if ( CALL_DATA == -1 )
		CALL_DATA = register_type(sizeof(call_data_t));

	return add_data((dataset_t*)&t->data, CALL_DATA);
}

void del_call_data(trace_t *t)
{
	del_data((dataset_t*)&t->data, CALL_DATA);
}

