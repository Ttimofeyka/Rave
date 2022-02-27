.extern _EPLf4main

.global _EPLf4exit
.type	_EPLf4exit, @function
_EPLf4exit:
	movl $1, %eax
	int $0x80

.global _start
.type	_start, @function
_start:
	call _EPLf4main
	
	push $1
	push $0x21
	call write

   	movl %eax, %ebx
	call exit
