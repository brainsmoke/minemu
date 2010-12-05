
/* This file is part of minemu
 *
 * Copyright 2010 Erik Bosman <erik@minemu.org>
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


#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "lib.h"
#include "error.h"

#include "syscalls.h"

void debug(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fd_vprintf(2, fmt, ap);
	va_end(ap);
	fd_printf(2, "\n");
}

int die(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fd_vprintf(2, fmt, ap);
	va_end(ap);
	fd_printf(2, "\n");
	raise(SIGKILL);
	return -1;
}
