#ifndef SCRATCH_H
#define SCRATCH_H

#define JMP_CACHE_SIZE (65536)
#define JMP_CACHE_MEM_SIZE (JMP_CACHE_SIZE*8)

#ifndef __ASSEMBLER__

typedef struct
{
	char *addr;
	char *jit_addr;

} jmp_map_t;

extern jmp_map_t jmp_cache[JMP_CACHE_SIZE];

extern unsigned long sigwrap_stack[];
extern unsigned long sigwrap_stack_bottom[];
extern unsigned long scratch_stack[];
extern char jit_fragment_page[];

extern long jit_fragment_exit_eip;
extern long jit_fragment_restartsys;
extern long jit_fragment_running;
extern long jit_fragment_scratch;
extern long user_eip;
extern long jit_eip;
extern long sysenter_reentry;

extern long ijmp_taint;

#ifdef EMU_DEBUG
extern char *last_jit;
extern long ijmp_misses;
extern long ijmp_count;
#endif

#endif

#endif /* SCRATCH_H */
