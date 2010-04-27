.text

.global dump_stack
.type dump_stack, @function
dump_stack:
push %ebp
mov  %esp,    %ebp
push %ebx
movl $0x4,    %eax
movl $0x1,    %ebx
lea  8(%ebp), %ecx
movl 8(%ebp), %edx
sub  %ecx,    %edx
int $0x80
pop %ebx
pop %ebp
ret

.global dump_stack_rev
.type dump_stack_rev, @function
dump_stack_rev:
push %ebp
mov  %esp,    %ebp
push %ebx
movl $0x1,    %ebx
mov  $0x1000, %edx
movl 8(%ebp), %ecx
loop:
sub  %edx,    %ecx
movl $0x4,    %eax
int $0x80
cmp %esp, %ecx
ja  loop
pop %ebx
pop %ebp
ret

