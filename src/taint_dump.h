
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

#ifndef TAINT_DUMP_H
#define TAINT_DUMP_H

#include "lib.h"

extern int dump_on_exit;
extern int dump_all;

void set_taint_dump_dir(const char *dir);
char *get_taint_dump_dir(void);

void stringdump_taint(int fd, const char *s, ssize_t len, const unsigned char *taint);

void hexdump_taint(int fd, const void *data, ssize_t len,
                           const unsigned char *taint, int offset, int ascii,
                           const char *descriptions[]);

void dump_map(int fd, char *addr, unsigned long len);

void do_regs_dump(int fd, long *regs);

void do_taint_dump(long *regs);

int open_dump_file(char *basename);

int open_taint_log(void);

#endif /* TAINT_DUMP_H */
