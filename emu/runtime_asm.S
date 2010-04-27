
.text
#include "scratch.h"

#
# enter(): Protect temu memory, load registers, jump to address
#
.global enter
.type enter, @function
enter:
movl 4(%esp), %eax         # instruction pointer
movl %eax, jit_eip
movl 8(%esp), %eax         # stack pointer
movl %eax, scratch_stack
movl $scratch_stack, %esp
call shield
xor %eax, %eax
xor %ecx, %ecx
xor %edx, %edx
xor %ebx, %ebx
xor %esi, %esi
xor %edi, %edi
xor %ebp, %ebp
push $0
popf
pop %esp
jmp jump_back_to_jit_code

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
cld
push %edi
push %ecx
mov jmp_list_size, %ecx
mov $jmp_list, %edi
test %edi, %edi          # clear ZF
repne scasl
jne,pn cache_miss

# address found in jmp_list,
# look up the mapping in the corresponding
# jit_address list
movl (JMP_LIST_SIZE*4-4)(%edi), %eax

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


#
# this symbol is used by the signal interceptor to set a breakpoint
# all runtime code must jump back to jit code using this instuction
#
.global jump_back_to_jit_code
jump_back_to_jit_code:

jmp *jit_eip

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
# syscall wrapper
#
runtime_syscall:
push %ebp
push %edi
push %esi
push %edx
push %ecx
push %ebx
push %eax
call syscall_emu
lea 28(%esp), %esp
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
pushf
push %edx
push %ebx
push %ecx
call runtime_syscall
pop %ecx
pop %ebx
pop %edx
popf
push %eax
pushf
movl jit_eip, %eax
test %eax, %eax
jz int80_address_lost
popf
pop %eax
pop %esp
jmp jump_back_to_jit_code
int80_address_lost:
movl user_eip, %eax
push $jit_eip
jmp runtime_ijmp

.global linux_sysenter_emu
.type linux_sysenter_emu, @function
linux_sysenter_emu:
movl (%esp), %ebp
movl %esp, scratch_stack
movl $scratch_stack, %esp
addl $12, (%esp)                     # user stack: %ebp  %edx  %ecx  <ret_addr>
pushf
call runtime_syscall
popf
push %eax
pushf
movl  8(%esp), %eax                 # user stack pointer

movl -12(%eax), %ebp
movl  -8(%eax), %edx
movl  -4(%eax), %ecx
movl    (%eax), %eax                 # return address

push $jit_eip
jmp runtime_ijmp

runtime_jit:
call unshield
movl 4(%esp), %eax
movl %esp, temu_stack_bottom
movl $temu_stack_bottom, %esp
push %eax
call jit
addl $4, %esp
pop %esp
push %eax
call shield
pop %eax
ret

