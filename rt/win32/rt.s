.extern _EPLf4main
.extern ExitProcess

.global _EPLf4exit
.type   _EPLf4exit, @function
_EPLf4exit:
 	movl %eax, %ecx
    call ExitProcess

.global _start
.type   _start, @function
_start:
	call _EPLf4main
    movl %eax, %ecx
    call ExitProcess
    