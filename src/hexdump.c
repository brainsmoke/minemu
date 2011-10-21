
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include "hexdump.h"

static inline long max(long a, long b) { return a>b ? a:b; }
static inline long min(long a, long b) { return a<b ? a:b; }

static const char *desc_color = "\033[0;34m", *reset = "\033[m";

/* Prints up to 16 characters in hexdump style with optional colors
 * if `ascii' is non-zero, an additional ascii representation is printed
 */
void hexdump_line(int fd, const void *data, ssize_t len,
                  int offset, int ascii,
                  const char *description,
                  const unsigned char *indices,
                  const char *colors[])
{
	int i, cur = -1;
	char c;

	if ( description != NULL )
		fd_printf(fd, "           %s%s%s\n", desc_color, description, reset);

	for (i=0; i<16; i++)
	{
		if (i == 0)
		{
			if (offset)
				fd_printf(fd, "%08x  ", data);
			else
				fd_printf(fd, "          ");
		}
		else if (i % 8 == 0)
			fd_printf(fd, " ");

		if (i < len)
		{
			if ( indices && colors && (cur != indices[i]) )
				fd_printf(fd, "%s", colors[cur = indices[i]]);

			fd_printf(fd, " %02x", ((unsigned char *)data)[i]);
		}
		else
			fd_printf(fd, "   ");
	}

	if (indices && colors)
		fd_printf(fd, "\033[m");

	if (ascii && len > 0)
	{
		cur = -1;
		fd_printf(fd, "   |");

		for (i=0; i<16; i++)
		{
			if (i == len)
				break;

			if ( indices && colors && (cur != indices[i]) )
				fd_printf(fd, "%s", colors[cur = indices[i]]);

			c = ((unsigned char *)data)[i];
			fd_printf(fd, "%c", isprint(c)?c:'.');
		}

		if (indices && colors)
			fd_printf(fd, "\033[m");

		fd_printf(fd, "|");
	}

	fd_printf(fd, "\n");
}

void hexdump(int fd, const void *data, ssize_t len,
             int offset, int ascii,
             const char *description[],
             const unsigned char *indices,
             const char *colors[])
{
	ssize_t row;

	for (row=0; row*16<len; row++)
		hexdump_line(fd, (char*)data+row*16, min(16, len-row*16),
		             offset, ascii,
		             description ? description[row] : NULL,
		             indices+row*16, colors);
}


void hexdump_diff(int fd, const void *data1, ssize_t len1,
                          const void *data2, ssize_t len2,
                          int grane, int offset, int ascii,
                          const char *description[])
{
	ssize_t row, i;

	enum { NODIFF=0, DIFF=1 };
	unsigned char d[16];
	int diff = 0;

	const char *color1[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[0;37m" };
	const char *color2[] = { [DIFF]="\033[1;33m", [NODIFF]="\033[1;30m" };
	ssize_t minlen = min(len1, len2);
	ssize_t maxlen = max(len1, len2);

	for (row=0; row*16<maxlen; row++)
	{
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

		hexdump_line(fd, (char*)data1+row*16, min(16, len1-row*16),
		             offset, ascii,
		             description ? description[row] : NULL, d, color1);
		hexdump_line(fd, (char*)data2+row*16, min(16, len2-row*16),
		             offset, ascii, NULL, d, color2);
	}
}

void hexdump_diff3(int fd, const void *old, ssize_t old_len,
                           const void *new1, ssize_t new_len1,
                           const void *new2, ssize_t new_len2,
                           int grane, int ascii, int offset,
                           const char *description[])
{
	ssize_t row, i;

	enum { NODIFF=0, DIFF1=1, DIFF2=2, SAMEDIFF=4 };
	unsigned char d[16];
	unsigned char dold[16] = { 0, };
	int diff = 0;

	const char *oldcolor[] = { [0]="\033[0;32m" };

	const char *color1[] =
	{
		[NODIFF]="\033[0;37m",
		[DIFF1]="\033[1;33m",
		[DIFF2]="\033[0;37m",
		[DIFF1|DIFF2]="\033[1;33m",
		[SAMEDIFF]="\033[1;37m",
	};

	const char *color2[] =
	{
		[NODIFF]="\033[1;30m",
		[DIFF1]="\033[1;30m",
		[DIFF2]="\033[1;33m",
		[DIFF1|DIFF2]="\033[1;33m",
		[SAMEDIFF]="\033[1;37m",
	};

	ssize_t maxlen = max(old_len, max(new_len1, new_len2));

	for (row=0; row*16<maxlen; row++)
	{
		for (i=0; i<16; i++)
		{
			if ( (row*16+i) % grane == 0 )
			{
				diff = NODIFF;

				if ( min(old_len -i-row*16, grane) !=
				     min(new_len1-i-row*16, grane) ||
				     memcmp( (char*)old+row*16+i,
				             (char*)new1+row*16+i,
				              min(grane, min(old_len-i-row*16,grane))) )
					diff |= DIFF1;

				if ( min(old_len -i-row*16, grane) !=
				     min(new_len2-i-row*16, grane) ||
				     memcmp( (char*)old+row*16+i,
				             (char*)new2+row*16+i,
				              min(grane, min(old_len-i-row*16,grane))) )
					diff |= DIFF2;

				if ( diff == (DIFF1|DIFF2) &&
				     (min(new_len1-i-row*16, grane) ==
				      min(new_len2-i-row*16, grane) &&
				     !memcmp( (char*)new1+row*16+i,
				              (char*)new2+row*16+i,
				               min(grane, min(new_len1-i-row*16,grane)))) )
					diff = SAMEDIFF;
			}

			d[i] = diff;
		}

		hexdump_line(fd, (char*)old+row*16, min(16, old_len-row*16),
		             offset, ascii,
		             description ? description[row] : NULL, dold, oldcolor);
		hexdump_line(fd, (char*)new1+row*16, min(16, new_len1-row*16),
		             offset, ascii, NULL, d, color1);
		hexdump_line(fd, (char*)new2+row*16, min(16, new_len2-row*16),
		             offset, ascii, NULL, d, color2);
	}
}

