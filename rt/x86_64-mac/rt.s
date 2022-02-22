.extern main

.global exit
exit:
	movq $0x2000001, %rax
	syscall

.global _start
_start:
	call main
	movq %rax, %rdi
	movq $0x2000001, %rax
	syscall
	