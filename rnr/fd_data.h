#ifndef FD_DATA_H
#define FD_DATA_H

#include "trace.h"
#include "util.h"

#define LINUX_MAX_FD (1024)

/* Flags for filedescriptors:
 *
 */

enum
{
	FILE_SYNC     = 0x01,
	FILE_SHADOW   = 0x02,
	FILE_READONLY = 0x04,
	FILE_CLOEXEC  = 0x08,
};

/* Datastructure to store data about filedescriptors
 * the structure may be shared between processes (threads)
 */

typedef struct
{
	unsigned int n_ref;
	unsigned char fd_flags[1024];

} fd_data_t;

/* void file_rename(...); */

fd_data_t *fd_data(trace_t *t);
fd_data_t *new_fd_data(trace_t *t);
fd_data_t *share_fd_data(trace_t *new, trace_t *old);
fd_data_t *unshare_fd_data(trace_t *t);
fd_data_t *copy_fd_data(trace_t *new, trace_t *old);
void del_fd_data(trace_t *t);

/* operations that affect filedescriptors and/or their paths
 *
 */
/*
void fd_dup(fd_data_t *d, long fd1, long fd2);
void fd_close(fd_data_t *d, long fd);
void fd_open(fd_data_t *d, int fd, const char *path, unsigned int flags);
void fd_exec(fd_data_t *d);
void fd_movedir(fd_data_t *d, const char *source, const char *dest);

void fs_setcwd(fd_data_t *d, const char *dirpath);

int fd_shadowed(fd_data_t *d, long fd);
int fd_used(fd_data_t *d, long fd);
*/
#endif /* FD_DATA_H */
