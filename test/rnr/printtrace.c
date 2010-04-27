#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include "debug.h"
#include "trace.h"
#include "util.h"
#include "debug_syscalls.h"
#include "serialize.h"

long min(long a, long b) { return a<b ? a:b; }

void print_caption(const char *s)
{
	printf("\033[0;31m%s\033[m\n", s);
}

void print_caption_hi(const char *s)
{
	printf("\033[1;31m%s\033[m\n", s);
}

void skip_event(in_stream_t *in)
{
	while ( peek_type(in) != EVENT_START &&
	        peek_type(in) != STREAM_END )
		skip_message(in);
}

void print_array(char *array[])
{
	for (; *array; array++)
		printf("- %s\n", *array);
}

void printtrace(in_stream_t *in, pid_t selectpid)
{
	long call, *args, argno;
	siginfo_t info;
	size_t argc, len;
	char *data;
	pid_t pid;
	char **array;

	debug_init(stdout);

	for (;;) switch (peek_type(in))
	{
		case EVENT_START:
			pid = read_event_start(in);
			if ( (selectpid == -1) || (selectpid == pid) )
			{
				print_caption("EVENT_START");
				printf("%d\n", pid);
			}
			else
				skip_event(in);
			continue;
		case EVENT_END:
			print_caption("EVENT_END");
			read_event_end(in);
			continue;
		case REGISTERS:
			print_caption("REGISTERS");
			print_registers(read_registers(in));
			continue;
		case CALL:
			print_caption("CALL");
			call = read_call(in, &args, &argc);
			print_call(call, args, argc);
			printf("\n");
			continue;
		case SIGNAL_EVENT:
			print_caption_hi("SIGNAL");
			printf("%s\n", signal_name(read_signal_event(in, &info)));
			printhex(&info, sizeof(info));
			continue;
		case TIMESTAMP:
			print_caption_hi("TIMESTAMP");
			printf("%llud\n", (unsigned long long)read_timestamp(in));
			continue;
		case RESULT:
			print_caption("RESULT");
			printf("%ld\n", read_result(in));
			continue;
		case USPACE_COPY:
			print_caption("USPACE_COPY");
			data = read_uspace_copy(in, &argno, &len);
			printf("arg(%ld)\n", argno);
			printhex(data, min(1024,len));
			if (len >  1024)
				printf("    ...\n");
			continue;
		case USPACE_VERIFY:
			print_caption("USPACE_VERIFY");
			data = read_uspace_verify(in, &argno, &len);
			printf("arg(%ld)\n", argno);
			printhex(data, len);
			continue;
/*		case SHADOW_OP:
			print_caption("SHADOW_OP");
			read_shadow_op(in);
*/		case FILEPATH:
			print_caption("FILEPATH");
			printf("%s\n", read_filepath(in));
			continue;
		case ARGS:
			print_caption("ARGS");
			array = read_args(in);
			print_array(array);
			free_string_array(array);
			continue;
		case ENV:
			print_caption("ENV");
			array = read_env(in);
			print_array(array);
			free_string_array(array);
			continue;
		case INVALID_TYPE:
			print_caption("INVALID_TYPE, exiting");
			continue;
		case STREAM_END:
			return;
		default:
			print_caption("UNKNOWN_TYPE");
			skip_message(in);
			continue;
	}
}

int main(int argc, char **argv)
{
	pid_t pid = -1;
	in_stream_t *in = open_in_stream(0, 1, 1);

	if (argc > 1)
		pid = atoi(argv[1]);

	printtrace(in, pid);

	close_in_stream(in);
	exit(EXIT_SUCCESS);
}
