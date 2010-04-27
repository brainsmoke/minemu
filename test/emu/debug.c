#define _LARGEFILE64_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "debug.h"

static FILE *out = NULL;

void debug_init(FILE *debug_out)
{
	out = debug_out;
}

const char *fxsave_desc[] =
{
	"    [fcw] [fsw] ft [] [fop]    [  fpuip  ] [ cs] [---]",
	"    [  fpu dp ] [ ds] [---]    [  mxcsr  ] [mxcsrmask]",
	"    [              ST0             ] [---------------]",
	"    [              ST1             ] [---------------]",
	"    [              ST2             ] [---------------]",
	"    [              ST3             ] [---------------]",
	"    [              ST4             ] [---------------]",
	"    [              ST5             ] [---------------]",
	"    [              ST6             ] [---------------]",
	"    [              ST7             ] [---------------]",
	"    [                      XMM0                      ]",
	"    [                      XMM1                      ]",
	"    [                      XMM2                      ]",
	"    [                      XMM3                      ]",
	"    [                      XMM4                      ]",
	"    [                      XMM5                      ]",
	"    [                      XMM6                      ]",
	"    [                      XMM7                      ]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
	"    [------------------------------------------------]",
};

const char *pushadfd_desc[] =
{
	"    [   eax   ] [   ecx   ]    [   edx   ] [   ebx   ]",
	"    [   esp   ] [   ebp   ]    [   esi   ] [   edi   ]",
	"    [  eflags ]",
};

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
			fprintf(out, "   ");

		if (i < len)
		{
			if ( indices && colors && (cur != indices[i]) )
				fprintf(out, "%s", colors[cur = indices[i]]);

			fprintf(out, " %02x", ((unsigned char *)data)[i]);
		}
		else
			fprintf(out, "   ");
	}

	if (indices && colors)
		fprintf(out, "\033[m");

	if (ascii && len > 0)
	{
		cur = -1;
		fprintf(out, "    |");

		for (i=0; i<16; i++)
		{
			if (i == len)
				break;

			if ( indices && colors && (cur != indices[i]) )
				fprintf(out, "%s", colors[cur = indices[i]]);

			c = ((unsigned char *)data)[i];
			fprintf(out, "%c", isprint(c)?c:'.');
		}

		if (indices && colors)
			fprintf(out, "\033[m");

		fprintf(out, "|");
	}

	fprintf(out, "\n");
}

static void printhex_descr(const void *data, ssize_t len, int ascii,
                           const char *descriptions[])
{
	ssize_t row;

	for (row=0; row*16<len; row++)
	{
		if ( descriptions != NULL )
			fprintf(out, "%s%s%s\n", hi, descriptions[row], reset);

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
			fprintf(out, "%s%s%s\n", hi, descriptions[row], reset);

		for (i=0; i<16; i++)
		{
			if ( (row*16+i) % grane == 0 )
			{
				if ( (minlen != maxlen && minlen-i-row*16 < grane) ||
				      bcmp( (char*)data1+row*16+i,
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

void print_fxsave(char *fxsave1, char *fxsave2)
{
	printhex_diff_descr(fxsave1, 288, fxsave2, 288, 1, 1, fxsave_desc);
}

void print_pushadfd(long *pushadfd1, long *pushadfd2)
{
	printhex_diff_descr((char*)pushadfd1, 36, (char*)pushadfd2, 36, 4, 1, pushadfd_desc);
}

static void printhex_taint_descr(const void *data, ssize_t len, const void *taint,
                                 const char *descriptions[])
{
	ssize_t row, i;

	int d[16];

	const char *color1[] = { [0]="\033[0;37m", [1]="\033[1;31m" };
	const char *color2[] = { [0]="\033[1;30m", [1]="\033[1;31m" };

	for (row=0; row*16<len; row++)
	{
		if ( descriptions != NULL )
			fprintf(out, "%s%s%s\n", hi, descriptions[row], reset);

		for (i=0; i<16; i++)
			d[i] = ((char*)taint)[row*16+i] ? 1:0;

		printhex_line((char*)data+row*16, min(16, len*16), 0, d, color1);
		printhex_line((char*)taint+row*16, min(16, len*16), 0, d, color2);
	}
}

void printhex_taint(const void *data, ssize_t len, const void *taint)
{
	printhex_taint_descr(data, len, taint, NULL);
}

void printregs_taint(const void *regs, const void *taint)
{
	printhex_taint_descr(regs, 32, taint, pushadfd_desc);
}

