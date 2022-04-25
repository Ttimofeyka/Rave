.extern _Ravef4main
.extern ExitProcess

.global _start
.type   _start, @function
_start:
	call _Ravef4main
    movl %eax, %ecx
    call ExitProcess
    