.extern _EPLf4main

.global _EPLf4exit
.type   _EPLf4exit, @function
_EPLf4exit:
	movq $0x2000001, %rax
	syscall

.global _start
.type   _start, @function
_start:
	call _EPLf4main
	movq %rax, %rdi
	movq $0x2000001, %rax
	syscall
	