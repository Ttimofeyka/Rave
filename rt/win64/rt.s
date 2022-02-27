.extern main
.extern ExitProcess

.global _EPLf4exit
_EPLf4exit:
    movq %rax, %rcx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call main
    movq %rax, %rcx
    call ExitProcess
    