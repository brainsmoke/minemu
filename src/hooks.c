
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 * Copyright 2011 Vrije Universiteit Amsterdam
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

#include <stddef.h>
#include <string.h>

#include "hooks.h"
#include "lib.h"
#include "mm.h"
#include "error.h"
#include "taint_dump.h"
#include "syscalls.h"

static struct
{
	char *name;
	hook_func_t func;	

} hook_map[] =
{
	{ .func = fmt_check, .name = "fmt_check:" },
	{ .func = ping, .name = "ping:" },
	{ .func = dump_regs, .name = "dump_regs:" },
	{ .func = fault, .name = "fault:" },
	{ .func = sqli_check, .name = "sqli_check:" },
	{ .func = NULL },
};

static char hooklist_buf[0x1000];
char *hooklist = NULL;

int parse_hook(char *s, char **endptr)
{
	char *next_colon = strchr(s, ':');
	if (!next_colon)
		return -1;

	hook_func_t func=NULL;
	int i;
	for (i=0; hook_map[i].func; i++)
		if ( strncmp(s, hook_map[i].name, next_colon-s+1) == 0 )
			func = hook_map[i].func;

	if (!func)
		return -1;

	s = next_colon;

	unsigned long long inode, dev, offset;
	unsigned long mtime;

	s++;
	inode = strtohexull(s, &s);

	if (!s || *s != ':')
		return -1;

	s++;
	dev = strtohexull(s, &s);

	if (!s || *s != ':')
		return -1;

	s++;
	mtime = (unsigned long)strtohexull(s, &s);

	if (!s || *s != ':')
		return -1;

	s++;
	offset = strtohexull(s, &s);

	if (!s || (*s != ',' && *s != '\0') )
		return -1;

	if (*s == ',')
		s++;

	if (register_hook(func, inode, dev, mtime, offset) < 0)
		return -1;

	if (endptr)
		*endptr = s;

	return 0;
}

int parse_hooklist(char *s)
{
	if (!hooklist)
		hooklist_buf[0]='\0';

	if (strlen(hooklist_buf)+1+strlen(s) > 0x1000 - 1)
	{
		debug("Error: hook list too long");
		return -1;
	}

	if (hooklist)
		strcat(hooklist_buf, ",");

	strcat(hooklist_buf, s);

	while (*s != '\0')
		if ( parse_hook(s, &s) < 0 )
		{
			debug("Error parsing hook list");
			return -1;
		}

	hooklist = hooklist_buf;
	return 0;
}

#define MAX_HOOKS (256)

hook_t hook_table[MAX_HOOKS];

int n_hooks = 0;

int register_hook(hook_func_t func, unsigned long long inode,
                                    unsigned long long dev,
                                    unsigned long mtime,
                                    unsigned long long offset)
{
	if (n_hooks >= MAX_HOOKS)
		return -1;

	hook_table[n_hooks] = (hook_t)
	{
		.func = func,
		.inode = inode,
		.dev = dev,
		.mtime = mtime,
		.offset = offset,
	};

	n_hooks++;

	return 0;
}

hook_func_t get_hook_func(code_map_t *map, unsigned long offset)
{
	int i;
	hook_t *h=hook_table;
	unsigned long long file_offset = (unsigned long long)map->pgoffset*0x1000 + offset;

	for (i=0; i<n_hooks; i++, h++)
		if ( (h->inode  == map->inode) &&
		     (h->mtime  == map->mtime) &&
		     (h->dev    == map->dev)   &&
		     (h->offset == file_offset) )
			return h->func;

	return NULL;
}

void dump_string_if_tainted(char *msg, char *s, int len)
{
	int i, taint=0;
	for (i=0; i<len; i++)
		taint |= s[TAINT_OFFSET+i];

	if (!taint)
		return;

	int fd = open_taint_log();

	if (fd < 0)
		return;

	fd_printf(fd, "Warning: %s: ", msg);
	stringdump_taint(fd, s, len, (const unsigned char *)s+TAINT_OFFSET);
	fd_printf(fd, "\n");
	sys_close(fd);
}

int fmt_check(long *regs)
{
	long *esp = (long *)regs[4];
	char *fmt = (char *)esp[2];
	int len = strlen(fmt);
	/* TODO: parse format string for errors */
	dump_string_if_tainted("tainted format string", fmt, len);
	return 0;
}

int sqli_check(long *regs)
{
	long *esp = (long *)regs[4];
	char *query = (char *)esp[2];
	char *len = (char *)esp[3];
	/* TODO: parse string for sqli's */
	dump_string_if_tainted("tainted sql query", query, len);
	return 0;
}

int ping(long *regs)
{
	debug("ping");
	return 0;
}

int fault(long *regs)
{
	return -1;
}

int dump_regs(long *regs)
{
	do_regs_dump(2, regs);
	debug("");
	return 0;
}
