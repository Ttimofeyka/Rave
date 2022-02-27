.extern _EPLf4main

.global exit
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
	