#ifndef SYSCALL_VALIDATE_H
#define SYSCALL_VALIDATE_H

#include "trace.h"

/* Checks whether the syscall has a filedescriptor as argument
 * which is in use by us (to transfer information)
 * TODO: doesn't handle the transfer of filedescriptors using sendmsg
 */
int uses_our_fd(trace_t *t);

#endif /* SYSCALL_VALIDATE_H */
