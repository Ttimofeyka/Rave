.extern _Ravef4main
.extern ExitProcess

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
    movq %rax, %rcx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call _Ravef4main
    movq %rax, %rcx
    call ExitProcess
    