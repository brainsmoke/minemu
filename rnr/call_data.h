#ifndef CALL_DATA_H
#define CALL_DATA_H

#include <linux/socket.h>
#include <sys/socket.h>

#include "trace.h"

/* Datastructure to hold user data that might be overwritten at
 * post call, but which is still needed to retrieve the written
 * user data
 */

typedef struct
{
	long sock_arg[8];       /* socket calls pass their arguments indirectly */
	size_t pre_len;         /* the value of socklen-like parameters before, */
	size_t post_len;        /* and after syscall execution                  */
	struct msghdr pre_msg;  /*                                              */
	struct msghdr post_msg; /*                                              */
	size_t iovcnt;
	struct iovec iovec[UIO_MAXIOV];
} call_data_t;

call_data_t *call_data(trace_t *t);

void load_pre_call_data(trace_t *t);
void load_post_call_data(trace_t *t);
void save_post_call_data(trace_t *t);

long get_call_data(trace_t *t, long which);

call_data_t *add_call_data(trace_t *t);
void del_call_data(trace_t *t);

#endif /* CALL_DATA_H */
