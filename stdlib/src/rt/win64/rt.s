.extern main
.extern ExitProcess

.global _start
_start:
	call main
    movq %rax, %rcx
    call ExitProcess
    