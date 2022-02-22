.extern main
.extern ExitProcess

.global exit
exit:
 	movl %eax, %ecx
    call ExitProcess

.global _start
_start:
	call main
    movl %eax, %ecx
    call ExitProcess
    