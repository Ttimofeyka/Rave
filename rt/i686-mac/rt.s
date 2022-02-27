.extern main

.global _EPLf4exit
.type   _EPLf4exit, @function
_EPLf4exit:
    movl $1, %eax
    int $0x80

.global write
write:
	movl $4, %eax
	movl $1, %ebx
	movl %ebx, %ecx
	movl $0, %edx

	int $0x80
	ret

.global _start
.type   _start, @function
_start:
    call main
    movl %eax, %ebx
    movl $1, %eax
    int $0x80
