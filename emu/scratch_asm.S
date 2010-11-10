#include "scratch.h"
.bss
#.balign 4096 # .{b,p2,}align doesn't seem to work

.lcomm fault_page_0, 4096
.global fault_page_0

.lcomm sigwrap_stack_bottom, 32764             # signal stack
.global sigwrap_stack_bottom                   #
.lcomm sigwrap_stack, 4                        #
.global sigwrap_stack                          #

.lcomm fault_page_1, 4096
.global fault_page_1

.lcomm scratch_stack_bottom, 32764             # runtime stack
.global scratch_stack_bottom                   #
.lcomm scratch_stack, 4                        #
.global scratch_stack                          #

.lcomm fault_page_2, 4096
.global fault_page_2

.lcomm jit_fragment_page, 4096
.global jit_fragment_page

.lcomm fault_page_3, 4096
.global fault_page_3

.lcomm jmp_cache, JMP_CACHE_MEM_SIZE
.global jmp_cache

.global scratch_xmm5
.lcomm scratch_xmm5, 16
.global scratch_xmm6
.lcomm scratch_xmm6, 16
.global scratch_xmm7
.lcomm scratch_xmm7, 16

.global taint_tmp
.lcomm taint_tmp, 4

.lcomm jit_fragment_running, 4
.global jit_fragment_running
.lcomm jit_fragment_restartsys, 4
.global jit_fragment_restartsys
.lcomm jit_fragment_saved_esp, 4
.global jit_fragment_saved_esp
.lcomm jit_fragment_scratch, 4
.global jit_fragment_scratch

.lcomm old_sigset, 8
.global old_sigset

.lcomm user_eip, 4
.global user_eip
.lcomm jit_eip, 4
.global jit_eip
.lcomm sysenter_reentry, 4
.global sysenter_reentry
#ifdef EMU_DEBUG
.lcomm ijmp_misses, 4
.global ijmp_misses
.lcomm ijmp_count, 4
.global ijmp_count
.lcomm last_jit, 4
.global last_jit
#endif

