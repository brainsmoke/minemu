
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

#ifndef DEBUG_H
#define DEBUG_H

#include <unistd.h>
#include <stdio.h>

/* Various debugging functions with neat color-coding */

/* must be set first */
void debug_init(FILE *debug_out);

void printhex(const void *data, int len);
void printhex_diff(const void *data1, ssize_t len1,
                   const void *data2, ssize_t len2, int grane);

void print_fxsave(char *fxsave1, char *fxsave2);
void print_pushadfd(long *pushadfd1, long *pushadfd2);

void printhex_taint(const void *data, ssize_t len, const void *taint);
void printregs_taint(const void *regs, const void *taint);

#endif /* DEBUG_H */
