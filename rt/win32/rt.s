.extern _Ravef4main
.extern ExitProcess

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
 	movl %eax, %ecx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call _Ravef4main
    movl %eax, %ecx
    call ExitProcess
    