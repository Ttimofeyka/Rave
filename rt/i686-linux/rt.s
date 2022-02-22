.extern main

.global exit
exit:
	movl $1, %eax
	int $0x80

.global _start
_start:
	call main
   	movl %eax, %ebx
	movl $1, %eax
	int $0x80
