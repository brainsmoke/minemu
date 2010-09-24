
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "debug.h"
#include "lib.h"
#include "scratch.h"
#include "error.h"

static int out = 2;

static const char *debug_desc[] =
{
	"    [ret miss ] [ret count]    [ijmp miss] [ijmpcount]",
	"    [ last_jit]",
};

/*
static FILE *out = NULL;

void debug_init(FILE *outfile)
{
	out = outfile;
}

FILE *debug_out(void)
{
	return out;
}

#ifdef __i386__

const char *registers_desc[] =
{
	"    [   ebx   ] [   ecx   ]    [   edx   ] [   esi   ]",
	"    [   edi   ] [   ebp   ]    [   eax   ] [ ds] [_ds]",
	"    [ es] [_es] [ fs] [_fs]    [ gs] [_gs] [ orig eax]",
	"    [   eip   ] [ cs] [_cs]    [  eflags ] [   esp   ]",
	"    [ ss] [_ss]",
};

#endif

#ifdef __x86_64__
#error structure has changed, needs to be retested

const char *registers_desc[] =
{
	"    [         r15         ]    [         r14         ]",
	"    [         r13         ]    [         r12         ]",
	"    [         rbp         ]    [         rbx         ]",
	"    [         r11         ]    [         r10         ]",
	"    [         r9          ]    [         r8          ]",
	"    [         rax         ]    [         rcx         ]",
	"    [         rsi         ]    [         rdi         ]",
	"    [       orig_rax      ]    [         rip         ]",
	"    [          cs         ]    [        eflags       ]",
	"    [         rsp         ]    [          ss         ]",
	"    [       fs_base       ]    [        gs_base      ]",
	"    [          ds         ]    [          es         ]",
	"    [          fs         ]    [          gs         ]",
};

#endif
*/

static inline long min(long a, long b) { return a<b ? a:b; }
static inline long max(long a, long b) { return a>b ? a:b; }

static const char *hi = "\033[0;34m", *reset = "\033[m";

/* Prints up to 16 characters in hexdump style with optional colors
 * if `ascii' is non-zero, an additional ascii representation is printed
 */
static void printhex_line(const void *data, ssize_t len, int ascii,
                          const int indices[], const char *colors[])
{
	int i, cur = -1;
	char c;

	for (i=0; i<16; i++)
	{
		if (i % 8 == 0)
			fd_printf(out, "   ");

		if (i < len)
		{
			if ( indices && colors && (cur != indices[i]) )
				fd_printf(out, "%s", colors[cur = indices[i]]);

			fd_printf(out, " %02x", ((unsigned char *)data)[i]);
		}
		else
			fd_printf(out, "   ");
	}

	if (indices && colors)
		fd_printf(out, "\033[m");

	if (ascii && len > 0)
	{
		cur = -1;
		fd_printf(out, "    |");

		for (i=0; i<16; i++)
		{
			if (i == len)
				break;

			if ( indices && colors && (cur != indices[i]) )
				fd_printf(out, "%s", colors[cur = indices[i]]);

			c = ((unsigned char *)data)[i];
			fd_printf(out, "%c", isprint(c)?c:'.');
		}

		if (indices && colors)
			fd_printf(out, "\033[m");

		fd_printf(out, "|");
	}

	fd_printf(out, "\n");
}

static void printhex_descr(const void *data, ssize_t len, int ascii,
                           const char *descriptions[])
{
	ssize_t row;

	for (row=0; row*16<len; row++)
	{
		if ( descriptions != NULL )
			fd_printf(out, "%s%s%s\n", hi, descriptions[row], reset);

		printhex_line((char*)data+row*16, min(16, len-row*16),
		              ascii, NULL, NULL);
	}
}

void printhex(const void *data, int len)
{
	printhex_descr(data, len, 1, NULL);
}

static void printhex_diff_descr(const void *data1, ssize_t len1,
                                const void *data2, ssize_t len2,
                                int grane, int ascii,
                                const char *descriptions[])
{
	ssize_t row, i;

	enum { NODIFF=0, DIFF=1 };
	int d[16], diff = 0;

	const char *color1[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[0;37m" };
	const char *color2[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[1;30m" };
	ssize_t minlen = min(len1, len2);
	ssize_t maxlen = max(len1, len2);

	for (row=0; row*16<maxlen; row++)
	{
		if ( descriptions != NULL )
			fd_printf(out, "%s%s%s\n", hi, descriptions[row], reset);

		for (i=0; i<16; i++)
		{
			if ( (row*16+i) % grane == 0 )
			{
				if ( (minlen != maxlen && minlen-i-row*16 < grane) ||
				      memcmp( (char*)data1+row*16+i,
				              (char*)data2+row*16+i,
				              min(grane, minlen-i-row*16)) )
					diff = DIFF;
				else
					diff = NODIFF;
			}

			d[i] = diff;
		}

		printhex_line((char*)data1+row*16, min(16, len1-row*16),
		              ascii, d, color1);
		printhex_line((char*)data2+row*16, min(16, len2-row*16),
		              ascii, d, color2);
	}
}

void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane)
{
	printhex_diff_descr(data1, len1, data2, len2, grane, 1, NULL);
}

#ifdef EMU_DEBUG
void print_debug_data(void)
{
	debug("counts/misses ijmp: %u/%u ret: %u/%u", ijmp_count, ijmp_misses, ret_count, ret_misses);
	printhex_descr(&ret_misses, 20, 0, debug_desc);
}
#endif

/*
void print_trace(trace_t *t)
{
	printhex_descr(t, sizeof(trace_t), 0, trace_desc);
}

void print_trace_diff(trace_t *new, trace_t *old)
{
	printhex_diff_descr(new, sizeof(trace_t),
	                    old, sizeof(trace_t), 4, 0, trace_desc);
}

void print_trace_if_diff(trace_t *new, trace_t *old)
{
	if ( memcmp(&new->regs, &old->regs, sizeof(registers_t)) != 0 )
		print_trace_diff(new, old);
}

void print_registers(registers_t *regs)
{
	printhex_descr(regs, sizeof(registers_t), 0, registers_desc);
}

void print_registers_diff(registers_t *new, registers_t *old)
{
	printhex_diff_descr(new, sizeof(registers_t),
	                    old, sizeof(registers_t), 4, 0, registers_desc);
}

void print_registers_if_diff(registers_t *new, registers_t *old)
{
	if ( memcmp(new, old, sizeof(registers_t)) != 0 )
		print_registers_diff(new, old);
}
*/

