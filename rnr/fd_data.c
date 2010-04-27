
#include <stdlib.h>
#include <string.h>

#include "trace.h"
#include "dataset.h"
#include "errors.h"
#include "fd_data.h"

#define LINUX_MAX_FD (1024)

static int FD_DATA = -1;

fd_data_t *fd_data(trace_t *t)
{
	return *(fd_data_t**)get_data((dataset_t*)&t->data, FD_DATA);
}

static fd_data_t *add_fd_data(trace_t *t, fd_data_t *d)
{
	if ( FD_DATA == -1 )
		FD_DATA = register_type(sizeof(fd_data_t*));

	*(fd_data_t**)add_data((dataset_t*)&t->data, FD_DATA) = d;
	d->n_ref++;

	return d;
}

void del_fd_data(trace_t *t)
{
	fd_data_t *d = fd_data(t);

	if ( d->n_ref > 1 )
		d->n_ref--;
	else
		free(d);

	del_data((dataset_t*)&t->data, FD_DATA);
}

static fd_data_t *alloc_fd_data(void)
{
	fd_data_t *d = try_malloc(sizeof(fd_data_t));
	memset(d, 0, sizeof(fd_data_t));
	return d;
}

fd_data_t *new_fd_data(trace_t *t)
{
	return add_fd_data(t, alloc_fd_data());
}

fd_data_t *share_fd_data(trace_t *new, trace_t *old)
{
	return add_fd_data(new, fd_data(old));
}

fd_data_t *unshare_fd_data(trace_t *t)
{
	fd_data_t *d_old = fd_data(t),
	          *d_new = d_old;

	if ( d_old->n_ref > 1 )
	{
		del_fd_data(t);
		d_new = new_fd_data(t);
		*d_new = *d_old;
		d_new->n_ref = 1;
	}

	return d_new;
}

fd_data_t *copy_fd_data(trace_t *new, trace_t *old)
{
	share_fd_data(new, old);
	return unshare_fd_data(new);
}

