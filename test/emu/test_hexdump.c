
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

#include <sys/mman.h>
#include <linux/personality.h>
#include <string.h>

#include "syscalls.h"
#include "hexdump.h"

const char *desc[] =
{
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
	"[   eax   ] [   ecx   ]  [   edx   ] [   ebx   ]",
	"[   esp   ] [   ebp   ]  [   esi   ] [   edi   ]",
};

/* not called main() to avoid warnings about extra parameters :-(  */
int minemu_main(int argc, char *argv[], char **envp, long *auxv)
{
	char old[256], new1[256], new2[256];

	int i;

	for (i=0; i<256; i++)
	{
		old[i]=new1[i]=new2[i]=i;
	}

	for (i=0; i<10; i++)
	{
		new1[i*16 + 1] = new2[i*16 + 1] = new1[i*16 + 5]= new2[i*16 + 9] = 3;
		                                  new2[i*16 + 5]= 5;
		                                  new1[i*16 + 13]= 5;
	}

	new2[223] = 34;

	hexdump_diff3(1, old, 256, new1, 200, new2, 224, 4, 1, 1, desc);
	hexdump_diff(1, old, 256, new2, 224, 4, 1, 1, desc);
	sys_exit(0);
}
