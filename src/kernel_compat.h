
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

#ifndef KERNEL_COMPAT_H
#define KERNEL_COMPAT_H

/* Declare our own definitions of structures that differ
 * between libc and the kernel interface.  Since we do direct
 * syscalls we need the kernel ABI definition.
 */

#define KERNEL_NSIG (64)
typedef struct
{
	unsigned long bitmask[KERNEL_NSIG/8/sizeof(long)];

} kernel_sigset_t;

struct kernel_ucontext
{
    unsigned long uc_flags;
    struct kernel_ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    kernel_sigset_t uc_sigmask;
};

typedef void (*kernel_sighandler_t)(int, siginfo_t *, void *);

struct kernel_old_sigaction
{
	kernel_sighandler_t handler;
	kernel_sigset_t mask;
	unsigned long flags;
	void (*restorer) (void);
};

struct kernel_sigaction {
	kernel_sighandler_t handler;
	unsigned long flags;
	void (*restorer) (void);
	kernel_sigset_t mask;
};

struct kernel_sigframe {
    char *pretcode;
    int sig;
    struct sigcontext sc;
    struct _fpstate fpstate_unused;
    unsigned long extramask[KERNEL_NSIG/8/sizeof(long)-1];
    char retcode[8];
	/* fpstate */
};

struct kernel_rt_sigframe {
    char *pretcode;
    int sig;
    siginfo_t *pinfo;
    void *puc;
    siginfo_t info;
    struct kernel_ucontext uc;
    char retcode[8];
	/* fpstate */
};

#undef st_atime
#undef st_mtime
#undef st_ctime
struct kernel_stat64 {
	unsigned long long	st_dev;
	unsigned char	__pad0[4];

	unsigned long	__st_ino;

	unsigned int	st_mode;
	unsigned int	st_nlink;

	unsigned long	st_uid;
	unsigned long	st_gid;

	unsigned long long	st_rdev;
	unsigned char	__pad3[4];

	long long	st_size;
	unsigned long	st_blksize;

	/* Number 512-byte blocks allocated. */
	unsigned long long	st_blocks;

	unsigned long	st_atime;
	unsigned long	st_atime_nsec;

	unsigned long	st_mtime;
	unsigned int	st_mtime_nsec;

	unsigned long	st_ctime;
	unsigned long	st_ctime_nsec;

	unsigned long long	st_ino;
};

#endif /* KERNEL_COMPAT_H */
