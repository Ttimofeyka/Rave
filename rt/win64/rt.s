.extern main
.extern ExitProcess

.global _start
_start:
	call main
    xor %rcx, %rcx
    call ExitProcess
    