.extern _Ravef4main

.global _Ravef4exit
.type	_Ravef4exit, @function
_Ravef4exit:
	movl $1, %eax
	int $0x80

.global _start
.type	_start, @function
_start:
	call _Ravef4main
	
	push $1
	push $0x21
	call write

   	movl %eax, %ebx
	call exit
