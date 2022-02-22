.extern main
.extern ExitProcess

.global exit
exit:
    movq %rax, %rcx
    call ExitProcess

.global _start
_start:
	call main
    movq %rax, %rcx
    call ExitProcess
    