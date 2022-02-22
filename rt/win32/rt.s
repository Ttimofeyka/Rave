.extern main
.extern ExitProcess

.global _start
_start:
	call main
    movd %eax, %ecx
    call ExitProcess
    