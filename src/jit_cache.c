
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


#include <linux/limits.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>

#include "mm.h"
#include "jit_cache.h"
#include "lib.h"
#include "syscalls.h"
#include "error.h"
#include "jit_code.h"
#include "jit_mm.h"
#include "jit.h"
#include "taint.h"
#include "kernel_compat.h"

static char cache_dir_buf[PATH_MAX+1] = { 0, };

static char *cache_dir = NULL;

unsigned long fd_filesize(int fd)
{
	struct kernel_stat64 s;
	if ( sys_fstat64(fd, &s) < 0 )
		die("fd_filesize: sys_fstat64() failed");

	return s.st_size;
}

static char *get_cache_filename(char *buf, code_map_t *map, int pid)
{
	buf[0] = '\x0';

	strcat(buf, cache_dir);
	strcat(buf, "/i");
	hexcat(buf, map->inode >> 32);
	hexcat(buf, map->inode & 0xffffffff);
	strcat(buf, "-d");
	hexcat(buf, map->dev >> 32);
	hexcat(buf, map->dev & 0xffffffff);
	strcat(buf, "-m");
	hexcat(buf, map->mtime);
	strcat(buf, "-a");
	hexcat(buf, (unsigned long)map->addr);
	strcat(buf, "-l");
	hexcat(buf, (unsigned long)map->len);
	strcat(buf, "-j");
	hexcat(buf, (unsigned long)map->jit_addr);
	strcat(buf, "-p");
	hexcat(buf, map->pgoffset);
	if ( call_strategy == LAZY_CALL )
		strcat(buf, "L");
	else if ( call_strategy == PREFETCH_ON_CALL )
		strcat(buf, "P");
	else if ( call_strategy == PRESEED_ON_CALL )
		strcat(buf, "S");

	if ( taint_flag == TAINT_OFF )
		strcat(buf, "N");

	if (pid > 0)
	{
		strcat(buf, "pid");
		numcat(buf, pid);
	}

	strcat(buf, ".jitcache");
	return buf;
}

void set_jit_cache_dir(const char *dir)
{
	if ( absdir(cache_dir_buf, dir) == 0 )
		cache_dir = cache_dir_buf;
	else
		cache_dir = NULL;
}

char *get_jit_cache_dir(void)
{
	return cache_dir;
}

int try_load_jit_cache(code_map_t *map)
{
	if ( (map->inode == 0) || (cache_dir == NULL) )
		return 0;
	
	char buf[PATH_MAX+1+1024];
	int fd = sys_open(get_cache_filename(buf, map, -1), O_RDONLY, 0);
	if (fd < 0)
		return -1;

	unsigned long size = fd_filesize(fd);

	char *addr = (char *)sys_mmap2(map->jit_addr, PAGE_NEXT(size),
	                               PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED,
	                               fd, 0);

	if (addr != map->jit_addr)
		die("try_load_jit_cache: mmap failed"); 

	sys_close(fd);

	jit_resize(map, size);
	return -1;
}

int try_save_jit_cache(code_map_t *map)
{
	if ( (map->inode == 0) || (cache_dir == NULL) )
		return 0;

	long ret = -1;

	char tmpfile_buf[PATH_MAX+1],
	     finalfile_buf[PATH_MAX+1];

	char *tmpfile   = get_cache_filename(tmpfile_buf, map, sys_gettid()),
	     *finalfile = get_cache_filename(finalfile_buf, map, -1);

	int fd = sys_open(tmpfile, O_RDWR|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		return fd;

	if ( sys_write(fd, map->jit_addr, map->jit_len) == (long)map->jit_len )
		ret = sys_rename(tmpfile, finalfile);

	sys_close(fd);
	return ret;
}

