#include "scratch.h"
.bss
#.balign 4096 # .{b,p2,}align doesn't seem to work

.lcomm stack_bottom, 32764                     # signal stack
.global stack_bottom                           #
.lcomm signal_stack, 4                         #
.global signal_stack                           #
.lcomm scratch_stack_bottom, 32764             # runtime stack
.global scratch_stack_bottom                   #
.lcomm scratch_stack, 4                        #
.global scratch_stack                          #

.lcomm fault_page_0, 4096
.global fault_page_0

.lcomm jit_block_page, 4096
.global jit_block_page

.lcomm fault_page_1, 4096
.global fault_page_1

.lcomm jmp_cache, JMP_CACHE_MEM_SIZE
.global jmp_cache
.lcomm jmp_list, JMP_LIST_MEM_SIZE
.global jmp_list
.lcomm jmp_list_size, 4
.global jmp_list_size

.lcomm jit_block_exit_eip, 4
.global jit_block_exit_eip

.lcomm old_sigset, 256
.global old_sigset
.lcomm signal_pending, 4
.global signal_pending

.lcomm user_eip, 4
.global user_eip
.lcomm jit_eip, 4
.global jit_eip
.lcomm sysenter_reentry, 4
.global sysenter_reentry
#ifdef EMU_DEBUG
.lcomm ret_misses, 4
.global ret_misses
.lcomm ret_count, 4
.global ret_count
.lcomm ijmp_misses, 4
.global ijmp_misses
.lcomm ijmp_count, 4
.global ijmp_count
.lcomm last_jit, 4
.global last_jit
#endif

