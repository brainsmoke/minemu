
#include <linux/unistd.h>
#include <sys/mman.h>

#include "util.h"
#include "syscall_validate.h"
#include "syscall_info.h"
#include "call_data.h"

/* this does not handle select _newselect poll pselect6 or ppoll
 * the worst that could happen is (not) blocking
 * TODO: doesn't handle the transfer of filedescriptors using sendmsg
 */
int uses_our_fd(trace_t *t)
{
	unsigned int fd_args = syscall_info(t)->fd_args;
	int i;

	for (i=0; fd_args; i++)
		if ( (1<<i) & fd_args )
		{
			if (is_pipe_channel(get_call_data(t, i)))
				return 1;
			fd_args &= ~(1<<i);
		}

	if ( (t->syscall == __NR_mmap2) && (MAP_ANONYMOUS & ~get_arg(t, 3)) &&
	     is_pipe_channel(get_arg(t, 4)) )
		return 1;

	return 0;
}

