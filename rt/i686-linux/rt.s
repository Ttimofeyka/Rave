.extern main

.global _EPLf4exit
.type	_EPLf4exit, @function
_EPLf4exit:
	movl $1, %eax
	int $0x80

.global _start
.type	_start, @function
_start:
	call main
   	movl %eax, %ebx
	movl $1, %eax
	int $0x80
