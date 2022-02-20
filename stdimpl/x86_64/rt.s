
.extern main

_start:
	call main
	movq %rax, %rdi
	movq %rax, $60
	syscall
