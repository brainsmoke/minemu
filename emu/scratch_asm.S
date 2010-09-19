#include "scratch.h"
.bss
.align 4
.lcomm stack_bottom, 65532
.global stack_bottom
.lcomm scratch_stack, 4
.global scratch_stack

.lcomm jmp_cache, JMP_CACHE_MEM_SIZE
.global jmp_cache
.lcomm jmp_list, JMP_LIST_MEM_SIZE
.global jmp_list
.lcomm jmp_list_size, 4
.global jmp_list_size
.lcomm tmp_list, JMP_LIST_MEM_SIZE
.global tmp_list
.lcomm tmp_list_size, 4
.global tmp_list_size
.lcomm user_eip, 4
.global user_eip
.lcomm jit_eip, 4
.global jit_eip
#ifdef EMU_DEBUG
.lcomm last_jit, 4
.global last_jit
#endif
