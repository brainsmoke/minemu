.global load_sigframe
.type load_sigframe, @function
load_sigframe:
movl 4(%esp), %eax
movl 8(%esp), %esp
lea  8(%esp), %esp
int $0x80
ud2

.global load_rt_sigframe
.type load_rt_sigframe, @function
load_rt_sigframe:
movl 4(%esp), %eax
movl 8(%esp), %esp
lea  4(%esp), %esp
int $0x80
ud2

