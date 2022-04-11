.extern _Ravef4main

.global _start
.type	_start, @function
_start:
	call _Ravef4main

   	movl %eax, %ebx
	movl $1, %eax
	int $0x80
