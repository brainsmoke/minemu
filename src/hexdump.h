
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

#ifndef HEXDUMP_H
#define HEXDUMP_H

#include "lib.h"

/* Prints up to 16 characters in hexdump style with optional colors
 * if `ascii' is non-zero, an additional ascii representation is printed
 */
void hexdump_line(int fd, const void *data, ssize_t len,
                  int offset, int ascii,
                  const char *description,
                  const unsigned char *indices,
                  const char *colors[]);

void hexdump(int fd, const void *data, ssize_t len,
             int offset, int ascii,
             const char *description[],
             const unsigned char *indices,
             const char *colors[]);

void hexdump_diff(int fd, const void *data1, ssize_t len1,
                          const void *data2, ssize_t len2,
                          int grane, int offset, int ascii,
                          const char *description[]);

void hexdump_diff3(int fd, const void *old, ssize_t old_len,
                           const void *new1, ssize_t new_len1,
                           const void *new2, ssize_t new_len2,
                           int grane, int ascii, int offset,
                           const char *description[]);

#endif /* HEXDUMP_H */
