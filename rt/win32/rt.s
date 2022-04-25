.extern ExitProcess

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
 	movl %eax, %ecx
    call ExitProcess
