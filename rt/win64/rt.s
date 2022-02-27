.extern _EPLf4main
.extern ExitProcess

.global _EPLf4exit
_EPLf4exit:
    movq %rax, %rcx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call _EPLf4main
    movq %rax, %rcx
    call ExitProcess
    