#include <sys/uio.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "errno.h"
#include "errors.h"
#include "serialize.h"

#include "debug.h"

/*#include <elf.h>*/
#include <linux/elf.h> /* for ELF_CLASS / ELF_DATA / ELF_ARCH */

static const char *type_names[] =
{
	[0+2] = "(UNKNOWN_MESSAGE)",
	[EVENT_START+2] = "EVENT_START",
	[REGISTERS+2] = "REGISTERS",
	[CALL+2] = "CALL",
	[RESULT+2] = "RESULT",
	[USPACE_COPY+2] = "USPACE_COPY",
	[USPACE_VERIFY+2] = "USPACE_VERIFY",
	[SIGNAL_EVENT+2] = "SIGNAL_EVENT",
	[TIMESTAMP+2] = "TIMESTAMP",
	[FILEPATH+2] = "FILEPATH",
	[ARGS+2] = "ARGS",
	[ENV+2] = "ENV",
	[EVENT_END+2] = "EVENT_END",
	[STREAM_END+2] = "STREAM_END",
	[INVALID_TYPE+2] = "INVALID_TYPE",
};
static const char *message_type(int type)
{
	if (type < INVALID_TYPE || type > EVENT_END )
		type = 0;

	return type_names[type+2];
}

static void try_write(int fd, void *buf, size_t count)
{
	if ( write(fd, buf, count) != count )
		fatal_error("write failed: %s", strerror(errno));
}

static const char *magic = "RNR";

typedef struct
{
	char magic[3];
	char version;
	char elf_class;
	char elf_data;
	short elf_arch;
} stream_hdr_t;

typedef struct
{
	short type, reserved;
	size_t payload_len;
} hdr_t;

#define IN_STREAM_BUF_INIT (4096)
struct in_stream_s
{
	hdr_t hdr;
	int fd;
	char *buf;
	size_t size;
};

struct out_stream_s
{
	int fd;
};

in_stream_t *open_in_stream(int fd, int minver, int maxver)
{
	in_stream_t *in = try_malloc(sizeof(in_stream_t));
	stream_hdr_t hdr;

	*in = (in_stream_t)
	{
		.hdr = { .type = INVALID_TYPE },
		.fd = fd,
		.size = IN_STREAM_BUF_INIT,
		.buf = try_malloc(IN_STREAM_BUF_INIT),
	};

	read(fd, &hdr, sizeof(stream_hdr_t));

	if ( bcmp(hdr.magic, magic, 3) ||
	     (hdr.version < minver) ||
	     (hdr.version > maxver) ||
	     (hdr.elf_class != ELF_CLASS) ||
	     (hdr.elf_data != ELF_DATA) ||
	     (hdr.elf_arch != ELF_ARCH) )
		fatal_error("invalid stream format");

	return in;
}

out_stream_t *open_out_stream(int fd, int version)
{
	out_stream_t *out = try_malloc(sizeof(out_stream_t));
	*out = (out_stream_t) { .fd = fd };

	stream_hdr_t hdr =
	{
		.version = version,
		.elf_class = ELF_CLASS,
		.elf_data = ELF_DATA,
		.elf_arch = ELF_ARCH	
	};
	memcpy(hdr.magic, magic, 3);

	try_write(fd, &hdr, sizeof(stream_hdr_t));

	return out;
}

void close_in_stream(in_stream_t *s)
{
	free(s->buf);
	free(s);
}

void close_out_stream(out_stream_t *s)
{
	free(s);
}

msg_type_t peek_type(in_stream_t *s)
{
	int n_read;

	if (s->hdr.type == INVALID_TYPE)
	{
		n_read = read(s->fd, &s->hdr, sizeof(hdr_t));

		if ( n_read == 0 )
			return (s->hdr.type = STREAM_END);

		if ( n_read != sizeof(hdr_t) )
			fatal_error("unexpected end of stream in message header: %s",
			            strerror(errno));
	}

	return s->hdr.type;
}

static ssize_t read_message(in_stream_t *s, msg_type_t type)
{
	peek_type(s);

	if ( s->hdr.type != type )
		fatal_error("expected event %s, got %s",
		            message_type(type), message_type(s->hdr.type));

	if ( s->size < s->hdr.payload_len )
	{
		s->size = s->hdr.payload_len;
		s->buf = try_realloc(s->buf, s->size);
	}

	ssize_t total = 0, n_read;

	while (total < s->hdr.payload_len)
	{
		n_read = read(s->fd, s->buf+total, s->hdr.payload_len-total);

		if ( n_read <= 0 )
			fatal_error("unexpected end of stream in message body: %s",
			            strerror(errno));

		total += n_read;
	}

	s->hdr.type = INVALID_TYPE;
	return s->hdr.payload_len;
}


void skip_message(in_stream_t *s)
{
	read_message(s, peek_type(s));
}


pid_t read_event_start(in_stream_t *s)
{
	if ( read_message(s, EVENT_START) != sizeof(pid_t) )
		fatal_error("event start frame has the wrong size");

	return *(pid_t *)s->buf;
}

void read_event_end(in_stream_t *s)
{
	if ( read_message(s, EVENT_END) != 0 )
		fatal_error("event end frame has non-zero payload");
}

registers_t *read_registers(in_stream_t *s)
{
	if ( read_message(s, REGISTERS) != sizeof(registers_t) )
		fatal_error("register frame has the wrong size");

	return (registers_t *)s->buf;
}

long read_signal_event(in_stream_t *s, siginfo_t *info)
{
	if ( read_message(s, SIGNAL_EVENT) != sizeof(long) + sizeof (siginfo_t) )
		fatal_error("signal frame has the wrong size");

	*info = *(siginfo_t*)(s->buf+sizeof(long));
	return *(long *)s->buf;
}

uint64_t read_timestamp(in_stream_t *s)
{
	if ( read_message(s, TIMESTAMP) != sizeof(uint64_t) )
		fatal_error("result frame has the wrong size");

	return *(uint64_t *)s->buf;
}

long read_result(in_stream_t *s)
{
	if ( read_message(s, RESULT) != sizeof(long) )
		fatal_error("result frame has the wrong size");

	return *(long *)s->buf;
}

long read_call(in_stream_t *s, long **args, size_t *argc)
{
	size_t size = read_message(s, CALL);
	if ( (size % sizeof(long)) != 0 )
		fatal_error("args frame has the wrong size");

	if (!argc || !args)
		fatal_error("argc and args need to be set");

	long *buf = (long *)s->buf;

	*argc = (size/sizeof(long))-1;
	*args = &buf[1];

	return buf[0];
}

static char *read_uspace(in_stream_t *s, long *argno, size_t *len, msg_type_t type)
{
	size_t l = read_message(s, type);

	if ( l < sizeof(size_t) )
		fatal_error("wrong size");

	if (len)
		*len = l - sizeof(size_t);

	if (argno)
		*argno = *(long*)s->buf;

	return &s->buf[sizeof(long)];
}

char *read_uspace_copy(in_stream_t *s, long *argno, size_t *len)
{
	return read_uspace(s, argno, len, USPACE_COPY);
}

char *read_uspace_verify(in_stream_t *s, long *argno, size_t *len)
{
	return read_uspace(s, argno, len, USPACE_VERIFY);
}

char *read_filepath(in_stream_t *s)
{
	read_message(s, FILEPATH);
	return s->buf;
}

static char **read_string_array(in_stream_t *s, msg_type_t type)
{
	char **array, *data;
	size_t i, n_args, l = read_message(s, type);

	if ( l != 0 )
	{
		if ( s->buf[l-1] != '\0')
			fatal_error("malformed message: %s", message_type(type));

		data = try_malloc(l);
		memcpy(data, s->buf, l);
	}
	else
		data = NULL;

	for (n_args=0, i=0; i<l; i++)
		if (data[i] == '\0')
			n_args++;

	/* one extra index for null-termination */
	array = try_malloc( (n_args+1) * sizeof(char*));
	array[0] = data;

	for (n_args=0, i=0; i<l; i++)
		if (data[i] == '\0')
		{
			n_args++;
			array[n_args] = &data[i+1];
		}

	/* null terminate array instead of pointing the last array index
	 * to the first element beyond the buffer
	 */
	array[n_args] = NULL;

	return array;
}

char **read_args(in_stream_t *s)
{
	return read_string_array(s, ARGS);
}

char **read_env(in_stream_t *s)
{
	return read_string_array(s, ENV);
}

void free_string_array(char **array)
{
	free(*array);
	free(array);
}

int read_steptrace(in_stream_t *s)
{
	size_t l = read_message(s, STEPTRACE);

	if ( l != sizeof(size_t) )
		fatal_error("wrong size");

	return s->buf[0] ? 1:0;
}

static void write_hdr_region(out_stream_t *s,
                             void *data, size_t len, msg_type_t type)
{
	hdr_t hdr = { .type = type, .payload_len = len };
	struct iovec vec[2] =
	{
		{ .iov_base = &hdr, .iov_len = sizeof(hdr) },
		{ .iov_base = data, .iov_len = len }
	};
	if ( writev(s->fd, vec, 2) != sizeof(hdr)+len )
		fatal_error("error while writing message: %s",
		            strerror(errno));
}

void write_event_start(out_stream_t *s, pid_t pid)
{
	struct { hdr_t hdr; pid_t pid; } msg =
	{
		.hdr = { .type = EVENT_START, .payload_len = sizeof(pid_t) },
		.pid = pid
	};
	try_write(s->fd, &msg, sizeof(msg));
}

void write_event_end(out_stream_t *s)
{
	hdr_t hdr = { .type = EVENT_END, .payload_len = 0 };
	try_write(s->fd, &hdr, sizeof(hdr));
}

void write_registers(out_stream_t *s, registers_t *regs)
{
	write_hdr_region(s, regs, sizeof(registers_t), REGISTERS);
}

static void write_long(out_stream_t *s, long val, msg_type_t type)
{
	struct { hdr_t hdr; long val; } msg =
	{
		.hdr = { .type = type, .payload_len = sizeof(pid_t) },
		.val = val
	};
	try_write(s->fd, &msg, sizeof(msg));
}

void write_result(out_stream_t *s, long result)
{
	write_long(s, result, RESULT);
}

void write_signal_event(out_stream_t *s, long signo, siginfo_t *info)
{
	struct { hdr_t hdr; long signo; siginfo_t info; } msg =
	{
		.hdr =
		{
			.type = SIGNAL_EVENT,
			.payload_len = sizeof(long)+sizeof(siginfo_t)
		},
		.signo = signo,
		.info = *info,
	};
	try_write(s->fd, &msg, sizeof(msg));
}

void write_timestamp(out_stream_t *s, uint64_t timestamp)
{
	write_hdr_region(s, &timestamp, sizeof(uint64_t), TIMESTAMP);
}

void write_call(out_stream_t *s, long call, long *args, size_t argc)
{
	struct { hdr_t hdr; long call; } msg =
	{
		.hdr = { .type = CALL, .payload_len = (argc+1)*+sizeof(long) },
		.call = call,
	};
	struct iovec vec[2] =
	{
		{ .iov_base = &msg, .iov_len = sizeof(msg) },
		{ .iov_base = args, .iov_len = argc*sizeof(long) }
	};
	if ( writev(s->fd, vec, 2) != sizeof(msg)+argc*sizeof(long) )
		fatal_error("error while writing message: %s",
		            strerror(errno));
}

void write_uspace(out_stream_t *s, long argno, void *buf, size_t len, msg_type_t type)
{
	struct { hdr_t hdr; long argno; } msg =
	{
		.hdr = { .type = type, .payload_len = len+sizeof(long) },
		.argno = argno,
	};
	struct iovec vec[2] =
	{
		{ .iov_base = &msg, .iov_len = sizeof(msg) },
		{ .iov_base = buf, .iov_len = len }
	};
	if ( writev(s->fd, vec, 2) != sizeof(msg)+len )
		fatal_error("error while writing message: %s",
		            strerror(errno));
}

void write_uspace_copy(out_stream_t *s, long argno, void *buf, size_t len)
{
	write_uspace(s, argno, buf, len, USPACE_COPY);
}

void write_uspace_verify(out_stream_t *s, long argno, void *buf, size_t len)
{
	write_uspace(s, argno, buf, len, USPACE_VERIFY);
}

void write_filepath(out_stream_t *s, char *shadow)
{
	write_hdr_region(s, shadow, strlen(shadow)+1, FILEPATH);
}

static void write_string_array(out_stream_t *s, char **array, msg_type_t type)
{
	size_t i, size=0;
	for (i=0; array[i]; i++)
		size += strlen(array[i]) + 1; /* count null-terminator */

	hdr_t hdr = { .type = type, .payload_len = size };
	try_write(s->fd, &hdr, sizeof(hdr));

	for (i=0; array[i]; i++)
		try_write(s->fd, array[i], strlen(array[i]) + 1);
}

void write_args(out_stream_t *s, char **array)
{
	write_string_array(s, array, ARGS);
}

void write_env(out_stream_t *s, char **array)
{
	write_string_array(s, array, ENV);
}

void write_steptrace(out_stream_t *s, int steptrace)
{
	size_t x = steptrace ? 1:0;
	write_hdr_region(s, &x, sizeof(size_t), STEPTRACE);
}
