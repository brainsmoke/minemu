.text
#include "scratch.h"

#
# emu_start(): Protect temu memory, load registers, jump to address
#
.global emu_start
.type emu_start, @function
emu_start: # (long eip, long esp)
movl 4(%esp), %esi           # load jit translated %eip from arg1
movl 8(%esp), %ebp           # load user's stack pointer from arg2
movl $scratch_stack+4, %esp  # set the stack to the scratch stack
push %ebp
call shield                # protect emulator memory
movl %esi, %eax
xor %ecx, %ecx             # clear registers and flags
xor %edx, %edx
xor %ebx, %ebx
xor %esi, %esi
xor %edi, %edi
xor %ebp, %ebp
push %ebx                  # cleared %eax
push %ebx                  # cleared %eflags
push $jit_eip              # jump to jit translated code
jmp runtime_ijmp


.global runtime_ret
.type runtime_ret, @function
runtime_ret:
#ifdef EMU_DEBUG
incl ret_misses
#endif
pop %ecx
push $jit_eip
jmp runtime_ijmp

#
# runtime_ijmp() is called from the jit code and has hackish calling
# conventions, optimized for small jit code and speed:
#
# Values on the stack (in push order)
#
# - original stack pointer
# - original %eax
# - flags
# - cache address where the jit address ought to be stored
#
# %eax contains user jump address
#
# all other registers should be preserved
#
# outcome of this routine is a resored state and a jump to the
# right jit code.

.global runtime_ijmp
.type runtime_ijmp, @function
runtime_ijmp:
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
popf
pop %eax
pop %esp
jmp *jit_eip

#
# this symbol is used by the signal interceptor to set a breakpoint
# all runtime code must jump back to jit code using this instuction
#

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

# Values on the stack (in push order)
#
# - original stack pointer
# - eax
# - original address following interrupt
# - jit address following interrupt
#
.global int80_emu
.type int80_emu, @function
int80_emu:
pop %eax
movl %eax, jit_eip
pop %eax
movl %eax, user_eip
pop %eax                # original %eax
call runtime_syscall
push %eax
pushf
movl jit_eip, %eax
test %eax, %eax
jz int80_address_lost
popf
pop %eax
pop %esp
jmp *jit_eip
int80_address_lost:
movl user_eip, %eax
push $jit_eip
jmp runtime_ijmp

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
movl (%esp), %ebp      # restore %ebp, (is used to store %esp during sysenter.)
movl %esp, scratch_stack
movl $scratch_stack, %esp
call runtime_syscall
push %eax
pushf
movl sysenter_reentry, %eax
push $jit_eip
jmp runtime_ijmp
