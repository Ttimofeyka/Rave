.extern main

_start:
	call main
	movd %eax, %ebx
	movd %eax, $1
	int $0x80
