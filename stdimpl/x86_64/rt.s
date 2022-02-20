.extern main

.global _start
_start:
	call main
	movq %rax, %rdi
	movq $60, %rax
	syscall
