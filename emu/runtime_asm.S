.text
#include "scratch.h"

#
# emu_start(): Protect temu memory, load registers, jump to address
#
.global emu_start
.type emu_start, @function
emu_start: # (long eip, long esp)
movl 4(%esp), %esi           # load user %eip from arg1
movl 8(%esp), %ebp           # load user's stack pointer from arg2

movl $scratch_stack, %esp    # move the stack out of emulator memory
call shield                  # protect emulator memory
movl %ebp, %esp              # load user stack

movl %esi, %eax              # user %eip
xor %ecx, %ecx               # clear registers and flags
xor %edx, %edx
xor %ebx, %ebx
xor %ebp, %ebp
xor %esi, %esi
xor %edi, %edi
movl %ecx, scratch_stack-4   # cleared eax
jmp runtime_ijmp

taint_fault:
int $3

#
# RET / ICALL / IJMP FAST PATH:
#
# runtime_ijmp() is called from the jit code
#
# processor state is the same as the processor state
# would be for the user process except that %eax
# contains the jump address and that the original
# eax is stored at scratch_stack-4
#
.global runtime_ijmp
.type runtime_ijmp, @function
runtime_ijmp:

mov %esp, scratch_stack
mov $scratch_stack-4, %esp
pushf
.global runtime_ret_cleanup
.type runtime_ret_cleanup, @function
runtime_ret_cleanup:

#ifdef EMU_DEBUG
incl ijmp_count
#endif

ptest  %xmm5, %xmm5
push   %ecx
jne,pn taint_fault
movzwl %ax, %ecx
cmpl jmp_list(,%ecx,4), %eax
jne,pn direct_miss
movl (JMP_LIST_SIZE*4)+jmp_list(, %ecx, 4), %eax
pop %ecx
popf
mov %eax, jit_eip
pop %eax
pop %esp

jmp *jit_eip            #  self-modifying code replaces this for jit_fragment and back
                        #  in case of a signal (*UGLY*)
.global runtime_exit_jmpaddr
runtime_exit_jmpaddr:

#
# END FAST PATH
#
#

direct_miss:
#ifdef EMU_DEBUG
incl ret_misses
#endif
pop %ecx
push $jit_eip

# look up the jump address in the jmp_list
push %edi
push %ecx
movzx %ax, %ecx                  # jump address & 0xffff -> %ecx
mov $jmp_list, %edi
lookup_loop:
cmpl %eax, (%edi, %ecx, 4)       # jmp_list.addr[%ecx]
je,pt found
testl $-1, (%edi, %ecx, 4)
jz,pn cache_miss
inc %cx                          # %ecx = (%ecx+1) % 0x10000
cmp %ax, %cx
je,pn cache_miss

jmp lookup_loop

found:
# address found in jmp_list,
# look up the mapping in the corresponding
# jit_address list
movl (JMP_LIST_SIZE*4)(%edi, %ecx, 4), %eax

# - save jit_address in cache
# - restore register state
# - jump to jit_address
jit_return:
mov 8(%esp), %edi
pop %ecx
mov %eax, (%edi)
pop %edi
addl $4, %esp
mov %eax, jit_eip

.global runtime_ijmp_critical_start
runtime_ijmp_critical_start:
testl $-1, jit_fragment_running
jnz interrupted
popf
pop %eax
pop %esp
.global runtime_ijmp_critical_end
runtime_ijmp_critical_end:
jmp *jit_eip

interrupted:
popf
pop %eax
pop %esp
jmp jit_fragment_exit

# address not found in jmp_list
cache_miss:
push %edx
push %ebx

push %eax            # addr
push %eax            # addr (function argument)
call jit_lookup_addr
addl $4, %esp        # pop function argument (undefined value)
test %eax, %eax      # jit_addr or NULL
jnz lookup_hit

# the code we're looking for has not been translated yet
push (%esp)
call runtime_jit
addl $4, %esp

lookup_hit:
pop %edx             # addr
push %eax            # jit_addr
push %eax            # jit_addr
push %edx            # addr
call add_jmp_mapping
addl $8, %esp        # unsafe jit_addr, addr
pop %eax             # addr

pop %ebx
pop %edx
jmp jit_return

#
# total miss, we have to translate some code
#
runtime_jit:
call unshield                       # unprotect jit stack
movl 4(%esp), %eax
movl %esp, temu_stack_bottom        # switch to jit stack
movl $temu_stack_bottom, %esp       #
push %eax                           # address to be translated
call jit
addl $4, %esp
pop %esp                            # revert to scratch stack
push %eax
call shield                         # protect jit stack
pop %eax                            # translated address is stored in %eax
ret

#
# syscall wrapper
#
runtime_syscall:
pushf
push %ecx
push %edx
push %ebp
push %edi
push %esi
push %edx
push %ecx
push %ebx
push %eax
call syscall_emu
lea 28(%esp), %esp
pop %edx
pop %ecx
popf
ret

.global int80_emu
.type int80_emu, @function
int80_emu:
movl %esp, scratch_stack
movl $scratch_stack, %esp
call runtime_syscall
pop %esp
movl %eax, scratch_stack-4
movl user_eip, %eax
jmp runtime_ijmp

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
movl (%esp), %ebp      # restore %ebp, (is used to store %esp during sysenter.)
movl %esp, scratch_stack
movl $scratch_stack, %esp
call runtime_syscall
pop %esp
movl %eax, scratch_stack-4
movl sysenter_reentry, %eax
jmp runtime_ijmp

.global state_restore
.type state_restore, @function
state_restore:
movl %eax, scratch_stack-4
movl user_eip, %eax
jmp runtime_ijmp

