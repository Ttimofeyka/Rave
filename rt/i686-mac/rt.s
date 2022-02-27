.extern main

.global exit
exit:
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
_start:
    call main
    movl %eax, %ebx
    movl $1, %eax
    int $0x80
