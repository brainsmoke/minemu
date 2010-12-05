.text
#include "scratch.h"

#
# emu_start(): Protect minemu memory, load registers, jump to address
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
push %edi
push %esi
push %ebp
push scratch_stack   # user %esp
push %ebx
push %edx
push %ecx
push scratch_stack-4 # user %eax
push %esp
movl %eax, user_eip
pextrd $0, %xmm5, ijmp_taint
call unshield
call do_taint_dump
#int $3
ud2

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
.balign 64
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
cmpl jmp_cache(,%ecx,8), %eax
jne,pn cache_lookup_loop
movl jmp_cache+4(, %ecx, 8), %eax

jit_return: # %eax contains jump address
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

#
# Processor state:
#
# runtime stack: (high to low)
#
# scratch_stack - 0: user %esp
#               - 4: user %eax
#               - 8: user %eflags
#               - C: user %ecx     <-- %esp
# 
# %eax: user jmp address
# %ecx: %ax
#
cache_lookup_loop:

#ifdef EMU_DEBUG
incl ijmp_misses
#endif

inc %cx                          # %ecx = (%ecx+1) % 0x10000 (next cache entry)
cmpl %eax, jmp_cache(, %ecx, 8)   # jmp_cache.addr[%ecx]
je,pt cache_hit
testl $-1, jmp_cache(, %ecx, 8)   # jmp_cache.addr[%ecx]
jz,pn cache_miss
cmp %ax, %cx
jne,pt cache_lookup_loop

cache_hit:
movl jmp_cache+4(, %ecx, 8), %eax
jmp jit_return

# address not found in jmp_cache
#
# Processor state:
#
# %ecx: clobbered
#
cache_miss:
push %edx
push %ebx

push %eax            # addr
push %eax            # addr
call jit_lookup_addr # (char *addr);
lea 4(%esp), %esp
test %eax, %eax      # jit_addr or NULL
jnz lookup_hit

# the code we're looking for has not been translated yet
call runtime_jit     # (char *addr);

lookup_hit:
lea 4(%esp), %esp
pop %ebx
pop %edx
jmp jit_return

#
# total miss, we have to translate some code
#
runtime_jit:
call unshield                       # unprotect jit stack
movl 4(%esp), %eax
movl %esp, minemu_stack_bottom      # switch to jit stack
movl $minemu_stack_bottom, %esp     #
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

