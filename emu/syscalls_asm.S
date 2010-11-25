.text

# Sysenter did not do much for performance.
# It works, as long as the vdso is intact.
# I decided not to use it because of its dependence on linux kernel tricks.

#define DO_SYSCALL_SYSENTER \
push $1f ;\
push %ecx ;\
push %edx ;\
push %ebp ;\
movl %esp, %ebp ;\
sysenter ;\
1:

#define DO_SYSCALL_INTR_PRE_SYSENTER \
push $1f ;\
push %ecx ;\
push %edx ;\
push %ebp ;\
movl %esp, %ebp

#define DO_SYSCALL_INTR_SYSENTER \
sysenter

#define DO_SYSCALL_INTR_RESTARTSYS_SYSENTER \
movl %ebp, %esp ;\
pop %ebp ;\
pop %edx ;\
pop %ecx ;\
lea 4(%esp), %esp ;\
1:

#define DO_SYSCALL_INT80 int $0x80
#define DO_SYSCALL_INTR_INT80 int $0x80
#define DO_SYSCALL_INTR_PRE_INT80 ;
#define DO_SYSCALL_INTR_RESTARTSYS_INT80 ;

#define DO_SYSCALL                  DO_SYSCALL_INT80
#define DO_SYSCALL_INTR             DO_SYSCALL_INTR_INT80
#define DO_SYSCALL_INTR_PRE         DO_SYSCALL_INTR_PRE_INT80
#define DO_SYSCALL_INTR_RESTARTSYS  DO_SYSCALL_INTR_RESTARTSYS_INT80

.global syscall0
.type syscall0, @function
syscall0:
push %ebp
movl %esp, %ebp
movl  8(%ebp),%eax
DO_SYSCALL
pop %ebp
ret

.global syscall1
.type syscall1, @function
syscall1:
push %ebp
movl %esp, %ebp
push %ebx
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
DO_SYSCALL
pop %ebx
pop %ebp
ret

.global syscall2
.type syscall2, @function
syscall2:
push %ebp
movl %esp, %ebp
push %ebx
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
DO_SYSCALL
pop %ebx
pop %ebp
ret

.global syscall3
.type syscall3, @function
syscall3:
push %ebp
movl %esp, %ebp
push %ebx
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
movl 20(%ebp),%edx
DO_SYSCALL
pop %ebx
pop %ebp
ret

.global syscall4
.type syscall4, @function
syscall4:
push %ebp
movl %esp, %ebp
push %ebx
push %esi
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
movl 20(%ebp),%edx
movl 24(%ebp),%esi
DO_SYSCALL
pop %esi
pop %ebx
pop %ebp
ret

.global syscall5
.type syscall5, @function
syscall5:
push %ebp
movl %esp, %ebp
push %ebx
push %esi
push %edi
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
movl 20(%ebp),%edx
movl 24(%ebp),%esi
movl 28(%ebp),%edi
DO_SYSCALL
pop %edi
pop %esi
pop %ebx
pop %ebp
ret

.global syscall6
.type syscall6, @function
syscall6:
push %ebp
movl %esp, %ebp
push %ebx
push %esi
push %edi
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
movl 20(%ebp),%edx
movl 24(%ebp),%esi
movl 28(%ebp),%edi
movl 32(%ebp),%ebp
DO_SYSCALL
pop %edi
pop %esi
pop %ebx
pop %ebp
ret

.global syscall_intr
.type syscall_intr, @function
syscall_intr:
push %ebp
movl %esp, %ebp
push %ebx
push %esi
push %edi
movl  8(%ebp),%eax
movl 12(%ebp),%ebx
movl 16(%ebp),%ecx
movl 20(%ebp),%edx
movl 24(%ebp),%esi
movl 28(%ebp),%edi
movl 32(%ebp),%ebp
DO_SYSCALL_INTR_PRE
.global syscall_intr_critical_start
syscall_intr_critical_start:
movb $1, jit_fragment_restartsys
testb $1, jit_fragment_running
jnz return
.global syscall_intr_critical_end
syscall_intr_critical_end:
DO_SYSCALL_INTR
return:
DO_SYSCALL_INTR_RESTARTSYS
pop %edi
pop %esi
pop %ebx
pop %ebp
ret

