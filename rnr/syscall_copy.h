#ifndef SYSCALL_COPY_H
#define SYSCALL_COPY_H

#include "serialize.h"

void serialize_post_call_data(out_stream_t *out, trace_t *t);

void unserialize_post_call_data(in_stream_t *in, trace_t *t);

void compare_user_data(in_stream_t *in, trace_t *t);

#endif /* SYSCALL_COPY_H */
