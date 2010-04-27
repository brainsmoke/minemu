.text
.global start
.type start, @function
start:
pop  %edx                   # argc -> %edx, argv -> %esp
lea  4(%esp, %edx, 4), %ebx # envp -> %ebx
xor  %eax,             %eax
xor  %ecx,             %ecx
dec  %ecx
cld
mov  %ebx,             %edi
repne scas %es:(%edi), %eax # auxv -> %edi
mov  %esp,             %eax # argv -> %eax
push %edi                   # push auxv
push %ebx                   # push envp
push %eax                   # push argv
push %edx                   # push argc
call temu_main
