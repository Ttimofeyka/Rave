.extern _EPLf4main
.extern ExitProcess

.global _EPLf4exit
_EPLf4exit:
 	movl %eax, %ecx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call _EPLf4main
    movl %eax, %ecx
    call ExitProcess
    