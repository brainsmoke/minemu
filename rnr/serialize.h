#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <sys/types.h>
#include <inttypes.h>
#include <signal.h>

#include "trace.h"

/* marshalling / unmarshalling of record / replay data */

typedef struct in_stream_s in_stream_t;
typedef struct out_stream_s out_stream_t;

in_stream_t *open_in_stream(int fd, int minver, int maxver);
out_stream_t *open_out_stream(int fd, int version);

void close_in_stream(in_stream_t *s);
void close_out_stream(out_stream_t *s);

typedef enum
{
	EVENT_START = 1,

	REGISTERS,
	CALL,
	RESULT,
	USPACE_COPY,
	USPACE_VERIFY,
	SIGNAL_EVENT,
	TIMESTAMP,
/*	SHADOW_OP,*/
	FILEPATH,
	ARGS,
	ENV,
	STEPTRACE,

	EVENT_END,
	STREAM_END = -1,
	INVALID_TYPE = -2,
} msg_type_t;
/*static const char *message_type(int type); */

msg_type_t peek_type(in_stream_t *s);
void skip_message(in_stream_t *s);


/* All functions except read_args() and read_env() use
 * the internal stream buffer for data storage, so make
 * sure you've copied the data before you read another
 * message.
 * the results of read_args() and read_env() must be freed
 * using free_string_array()
 *
 * calling skip_message() on an ARGS or ENV message wont require
 * you to call free_string_array()
 */

pid_t read_event_start(in_stream_t *s);
void read_event_end(in_stream_t *s);
registers_t *read_registers(in_stream_t *s);
long read_result(in_stream_t *s);
long read_signal_event(in_stream_t *s, siginfo_t *info);
uint64_t read_timestamp(in_stream_t *s);
long read_call(in_stream_t *s, long **args, size_t *argc);
char *read_uspace_copy(in_stream_t *s, long *argno, size_t *len);
char *read_uspace_verify(in_stream_t *s, long *argno, size_t *len);
/*void read_shadow_op(in_stream_t *s);*/
char *read_filepath(in_stream_t *s);

/* must be called to free string arrays from read_args() and read_env() */
void free_string_array(char **array);

char **read_args(in_stream_t *s);
char **read_env(in_stream_t *s);
int read_steptrace(in_stream_t *s);



/*
struct stat64;
struct stat64 *read_stat(in_stream_t *s);
*/

void write_event_start(out_stream_t *s, pid_t pid);
void write_event_end(out_stream_t *s);
void write_registers(out_stream_t *s, registers_t *regs);
void write_call(out_stream_t *s, long call, long *args, size_t argc);
void write_result(out_stream_t *s, long result);
void write_signal_event(out_stream_t *s, long signal, siginfo_t *info);
void write_timestamp(out_stream_t *s, uint64_t timestamp);
void write_uspace_copy(out_stream_t *s, long argno, void *buf, size_t len);
void write_uspace_verify(out_stream_t *s, long argno, void *buf, size_t len);
/*void write_shadow_op(out_stream_t *s);*/
void write_filepath(out_stream_t *s, char *path);
/*
void write_stat(out_stream_t *s, struct stat64 *buf);
*/

void write_args(out_stream_t *s, char **array);
void write_env(out_stream_t *s, char **array);
void write_steptrace(out_stream_t *s, int steptrace);

#endif /* SERIALIZE_H */
