#include <elf.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <linux/limits.h>

#include "error.h"
#include "syscalls.h"
#include "load_elf.h"
#include "lib.h"
#include "mm.h"

#ifndef AT_EXECFN
#define AT_EXECFN 31
#endif
#ifndef AT_BASE_PLATFORM
#define AT_BASE_PLATFORM 24
#endif
#ifndef AT_RANDOM
#define AT_RANDOM 25
#endif

/* relocate the stack */

static long get_stack_random_shift(char **envp)
{
	char *max = 0;
	for ( ; *envp ; envp++ )
		if ( *envp > max )
			max = *envp;

	if (max == 0)
		return 0;
	else
		return 0x1000000 - (PAGE_NEXT((long)max) & 0xfff000);
}

static long auxv_count(long *auxv)
{
	long c=0;
	for (;auxv[c*2];c++);
	return c;
}

static long *find_aux(long *auxv, long id)
{
	for ( ; *auxv; auxv+=2)
		if (*auxv == id)
			return auxv+1;

	return NULL;
}

void set_aux(long *auxv, long id, long val)
{
	long *p = find_aux(auxv, id);
	if (p) *p = val;
}

long get_aux(long *auxv, long id)
{
	long *p = find_aux(auxv, id);
	if (p)
		return *p;
	else
		return 0;
}

static long strings_count(char **s)
{
	long c=0;
	for (;s[c];c++);
	return c;
}

static char *stack_push_string(char *sp, const char *s)
{
	return strcpy(sp -= strlen(s)+1, s);
}

static char *stack_push_strings(char *sp, char **dst, char **src)
{
	long i, c=strings_count(src);
	dst[c] = NULL;

	for (i=c-1; i>=0; i--)
		sp = dst[i] = stack_push_string(sp, src[i]);

	return sp;
}

static char *stack_push_data(char *sp, void *src, size_t n)
{
	return memcpy(sp -= n, src, n);
}

static int init_user_stack(elf_prog_t *prog, int prot)
{
	long err = do_mmap2(prog->task_size-prog->stack_size, prog->stack_size,prot,
	                    MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);

	if (err & PG_MASK)
		return -1;

	long argc = strings_count(prog->argv),
	     envc = strings_count(prog->envp),
	     auxc = auxv_count(prog->auxv);

	char *tmp_argv[argc+1],
	     *tmp_envp[envc+1],
	     *platform      = (char *)get_aux(prog->auxv, AT_PLATFORM),
	     *base_platform = (char *)get_aux(prog->auxv, AT_BASE_PLATFORM),
	     *rand_bytes    = (char *)get_aux(prog->auxv, AT_RANDOM);

	char *sp = (char *)(prog->task_size-get_stack_random_shift(prog->envp));

	sp -= sizeof(long);
	sp = prog->filename = stack_push_string(sp, prog->filename);
	sp = stack_push_strings(sp, tmp_envp, prog->envp);
	sp = stack_push_strings(sp, tmp_argv, prog->argv);
	sp = (char *)(((long)sp-0x100)&~0xf);

	if (platform)
		sp = platform      = stack_push_string(sp, platform);
	if (base_platform)
		sp = base_platform = stack_push_string(sp, base_platform);
	if (rand_bytes)
		sp = rand_bytes    = stack_push_data(sp, rand_bytes, 16);

	sp = (char *)((long)sp&~0xf);

	sp = stack_push_data(sp, prog->auxv, (auxc+1)*2*sizeof(long));
	prog->auxv = (long *)sp;

	sp = stack_push_data(sp, tmp_envp, (envc+1)*sizeof(char *));
	prog->envp = (char **)sp;

	sp = stack_push_data(sp, tmp_argv, (argc+1)*sizeof(char *));
	prog->argv = (char **)sp;

	sp = stack_push_data(sp, &argc, sizeof(long));

	set_aux(prog->auxv, AT_EXECFN, (long)prog->filename);
	set_aux(prog->auxv, AT_PLATFORM, (long)platform);
	set_aux(prog->auxv, AT_BASE_PLATFORM, (long)base_platform);
	set_aux(prog->auxv, AT_RANDOM, (long)rand_bytes);
	set_aux(prog->auxv, AT_PHDR, (long)prog->bin.phdr);
	set_aux(prog->auxv, AT_PHNUM, prog->bin.hdr.e_phnum);
	set_aux(prog->auxv, AT_BASE, prog->bin.base);
	set_aux(prog->auxv, AT_ENTRY, prog->bin.base + prog->bin.hdr.e_entry);

	prog->sp = (long *)sp;
	return 0;
}

/* loading the binary */

static long zerofill(unsigned long start, unsigned long end, int prot)
{
	unsigned long pg_start = PAGE_NEXT(start),
	              pg_end   = PAGE_NEXT(end),
	              padd_len = pg_start-start;

	if (start > end)
		return -1;

	if (prot & PROT_WRITE)
		memset((void *)start, 0, padd_len);

	return do_mmap2(pg_start, pg_end-pg_start, prot,
	                MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
}

static int open_exec(const char *filename)
{
	int ret, fd;

	if ( (ret = sys_open(filename, O_RDONLY, 0)) < 0 )
		return ret;

	fd = ret;

	/* fugly, but it works */
	char fd_filename[32] = "/proc/self/fd/";
	numcat(fd_filename, fd);

	if ( (ret = sys_access(fd_filename, X_OK)) < 0 )
	{
		sys_close(fd);
		return ret;
	}

	return fd;
}

static int read_elf_header(int fd, Elf32_Ehdr *hdr)
{
	ssize_t sz = read_at(fd, 0, hdr, sizeof(*hdr));

	if (sz < 0)
		return sz;

	if (sz != sizeof(*hdr))
		return -EIO;

	if (  (memcmp(&hdr[0], ELFMAG, SELFMAG) != 0) ||
	     ((hdr->e_type != ET_EXEC) &&
		  (hdr->e_type != ET_DYN)) ||
	     ((hdr->e_machine != EM_386) &&
		  (hdr->e_machine != 6 /* EM_486 */)) ||
	      (hdr->e_phentsize != sizeof(Elf32_Phdr)) ||
	      (hdr->e_phnum < 1) ||
	      (hdr->e_phentsize*hdr->e_phnum > 65536) )
		return -ENOEXEC;

	return 0;
}

static int find_interp_name(elf_bin_t *bin, char *buf, long max_size)
{
	long i;
	Elf32_Phdr *p;

	for (i=0; i<bin->hdr.e_phnum; i++) 
	{
		p = &bin->phdr[i];

		if (p->p_type == PT_INTERP)
		{
			if ( (p->p_filesz > max_size) || (p->p_filesz < 2) ) 
				return -ENOEXEC;

			if ( read_at(bin->fd, p->p_offset, buf, p->p_filesz) != p->p_filesz )
				return -EIO;

			if ( buf[p->p_filesz-1] != '\0' )
				return -ENOEXEC;

			return 1;
		}
	}

	return 0;
}

int open_elf(const char *filename, elf_bin_t *bin)
{
	int err;

	bin->fd = -1;

	if ( (err = open_exec(filename)) < 0 )
		return err;

	bin->fd = err;

	if ( (err = read_elf_header(bin->fd, &bin->hdr)) < 0 )
		return err;

	return 0;
}

static Elf32_Phdr *mapped_phdr(elf_bin_t *bin)
{
	int i;
	long offset;
	Elf32_Phdr *p;

	for (i=0; i<bin->hdr.e_phnum; i++) 
	{
		offset = bin->hdr.e_phoff;
		p = &bin->phdr[i];

		if ( (p->p_type == PT_LOAD) &&
		     (p->p_offset <= offset) &&
		     (p->p_offset+p->p_filesz > offset) )
			return (Elf32_Phdr *)(p->p_vaddr - p->p_offset + bin->base + offset);
	}

	return NULL;
}

static unsigned long set_brk(elf_bin_t *bin)
{
	long new_brk = 0, v_end, i;

	for (i=0; i<bin->hdr.e_phnum; i++) 
		if ( bin->phdr[i].p_type == PT_LOAD)
		{
			v_end = PAGE_NEXT(bin->phdr[i].p_vaddr+bin->phdr[i].p_memsz);

			if ( (bin->phdr[i].p_type == PT_LOAD) && (new_brk < v_end) )
				new_brk = v_end;
		}

	return (new_brk == set_brk_min(new_brk)) ? 0 : -1;
}

static long get_stack_prot(elf_bin_t *bin)
{
	int prot = PROT_READ|PROT_WRITE, i;

	for (i=0; i<bin->hdr.e_phnum; i++) 
		if ( (bin->phdr[i].p_type == PT_GNU_STACK) &&
		     (bin->phdr[i].p_flags & PF_X) )
			return prot|PROT_EXEC;

	return prot;
}

static unsigned long get_mmap_base(elf_bin_t *elf)
{
	unsigned long addr, pg_start, pg_end, mmap_min = ULONG_MAX, mmap_max = 0;
	int i;

	for (i=0; i<elf->hdr.e_phnum; i++) 
		if ( elf->phdr[i].p_type == PT_LOAD)
		{
			pg_start = PAGE_BASE(elf->phdr[i].p_vaddr);
			pg_end   = PAGE_NEXT(elf->phdr[i].p_vaddr + elf->phdr[i].p_memsz);

			if (pg_start < mmap_min)
				mmap_min = pg_start;

			if (pg_end > mmap_max)
				mmap_max = pg_end;
		}

	if (mmap_min > mmap_max)
		mmap_min = mmap_max;

	addr = do_mmap2(mmap_min, mmap_max-mmap_min, PROT_NONE,
	                MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	if (addr & PG_MASK) /* not on page boundary -> error code */
		return addr;

	sys_munmap(mmap_min, mmap_max-mmap_min);

	return addr-mmap_min;
}

static long mmap_prog_section(elf_bin_t *elf, Elf32_Phdr *p)
{
	unsigned long base = elf->base, brk_, bss, addr, pg_off, size;
	int prot = 0;

	if (p->p_flags & PF_R)
		prot |= PROT_READ;		
	if (p->p_flags & PF_W)
		prot |= PROT_WRITE;		
	if (p->p_flags & PF_X)
		prot |= PROT_EXEC;

	addr = PAGE_BASE(base+p->p_vaddr);
	size = PAGE_NEXT(base+p->p_vaddr + p->p_filesz) - addr;
	pg_off = p->p_offset/PG_SIZE;

	bss = base + p->p_vaddr + p->p_filesz;
	brk_ = base + p->p_vaddr + p->p_memsz;

	if ( ((p->p_vaddr-p->p_offset) & PG_MASK) || (bss > brk_) )
		return -1;

	addr = do_mmap2(addr, size, prot, MAP_PRIVATE|MAP_FIXED, elf->fd, pg_off);

	if (addr & PG_MASK) /* not on page boundary -> error code */
		return addr;

	if (elf->brk < brk_)
		elf->brk = brk_;

	if (elf->bss < bss)
		elf->bss = bss;

	/* linux does not fill in bss sections
	 * between load segments for interpreters;
	 * makes no difference to the standard ld.so
	 */
	addr = zerofill(bss, brk_, prot);

	if (addr & PG_MASK) /* not on page boundary -> error code */
		return addr;

	return 0;
}

static long mmap_binary(elf_bin_t *elf, int is_interp)
{
	int i, ret;

	if (elf->hdr.e_type == ET_DYN)
		elf->base = get_mmap_base(elf);
	else
		elf->base = 0;

	if (elf->base & PG_MASK) /* not on page boundary -> error code */
		return -1;

	if (!is_interp)
		set_brk(elf); /* do this before mapping, memory gets scrubbed */

	for (i=0; i<elf->hdr.e_phnum; i++) 
		if ( (elf->phdr[i].p_type == PT_LOAD) &&
		     ((ret=mmap_prog_section(elf, &elf->phdr[i])) & PG_MASK) )
			return ret;

	return elf->base;
}

static int try_load_elf(elf_prog_t *prog)
{
	int err, has_interp;
	char i_filename[PATH_MAX]; /* so much easier */
	elf_bin_t *bin=&prog->bin, *interp=&prog->interp;

	if ( (err = open_elf(prog->filename, bin)) < 0 )
		return err;

	Elf32_Phdr phdr_bin_tmp[bin->hdr.e_phnum];
	bin->phdr = phdr_bin_tmp; /* point to mmapped region later if any */

	if (read_at(bin->fd, bin->hdr.e_phoff, bin->phdr,
			sizeof(*bin->phdr)*bin->hdr.e_phnum) !=
			sizeof(*bin->phdr)*bin->hdr.e_phnum)
		return -EIO;

	if ( (err = find_interp_name(bin, i_filename, PATH_MAX)) < 0 )
		return err;

	has_interp = err;
	interp->phdr = NULL;
	interp->hdr.e_phnum = 0;

	if ( has_interp && (err = open_elf(i_filename, interp)) < 0)
		return err;

	Elf32_Phdr phdr_interp_tmp[interp->hdr.e_phnum];

	if ( has_interp )
	{
		interp->phdr = phdr_interp_tmp; /* point to mmapped region later */

		if (read_at(interp->fd, interp->hdr.e_phoff, interp->phdr,
				sizeof(*interp->phdr)*interp->hdr.e_phnum) !=
				sizeof(*interp->phdr)*interp->hdr.e_phnum)
			return -EIO;
	}

	/* point of no return */

	if ( mmap_binary(bin, 0) & PG_MASK )
		raise(SIGKILL);

	if ( has_interp && (mmap_binary(interp, 1) & PG_MASK) )
		raise(SIGKILL);

	/* set up stack */
	int stack_prot = get_stack_prot(bin);

	bin->phdr = mapped_phdr(bin);

	if ( has_interp )
		interp->phdr = mapped_phdr(interp);
	
	err = init_user_stack(prog, stack_prot);

	if (err & PG_MASK)
    	raise(SIGKILL);

	if (has_interp)
		prog->entry = (void *)(interp->base + interp->hdr.e_entry);
	else
		prog->entry = (void *)(bin->base + bin->hdr.e_entry);

	return 0;
}

int load_elf(elf_prog_t *prog)
{
	int err = try_load_elf(prog);

	if (prog->bin.fd != -1)
		sys_close(prog->bin.fd);

	if (prog->interp.fd != -1)
		sys_close(prog->interp.fd);

	return err;
}

