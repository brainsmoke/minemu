#ifndef SCRATCH_H
#define SCRATCH_H

#define JMP_CACHE_SIZE (65536)
#define JMP_LIST_SIZE (65536)
#define N_SYSCALL_HOOKS (400)
#define EMU_BIT (0x01)

#define JMP_CACHE_MEM_SIZE (JMP_CACHE_SIZE*8)
#define JMP_LIST_MEM_SIZE (JMP_LIST_SIZE*8)

#ifndef __ASSEMBLER__

typedef struct
{
	char *addr, *jit_addr;

} jmp_map_t;

extern jmp_map_t jmp_cache[JMP_CACHE_SIZE];

typedef struct
{
	char *addr[JMP_LIST_SIZE];
	char *jit_addr[JMP_LIST_SIZE];

} jmp_list_t;

extern jmp_list_t jmp_list;

extern unsigned long jmp_list_size;

extern unsigned long scratch_stack[];

extern long user_eip;
extern long jit_eip;
extern long sysenter_reentry;

#ifdef EMU_DEBUG
extern char *last_jit;
extern long ret_misses;
extern long ret_count;
extern long ijmp_misses;
extern long ijmp_count;
#endif

#endif

#endif /* SCRATCH_H */
