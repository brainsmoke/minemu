
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

#include <linux/personality.h>
#include <unistd.h>
unsigned int personality(unsigned long persona);

int main(int argc, char **argv)
{
	unsigned int p = personality(0xffffffff);
	if (p&ADDR_COMPAT_LAYOUT)
	{
		write(1, (char *)0x4001c000, 0x1000);
	}
	else
	{
		personality(p|ADDR_COMPAT_LAYOUT);
		execvp("/proc/self/exe", argv);
	}
}
