.extern _Ravef4main

.global _Ravef4exit
.type	_Ravef4exit, @function
_Ravef4exit:
	movl $1, %eax
	int $0x80

.global _Ravef5write
.type    _Ravef5write, @function
_Ravef5write:
	movl $1, %eax
	int $0x80
	ret

.global _start
.type	_start, @function
_start:
	call _Ravef4main

   	movl %eax, %ebx
	call _Ravef4exit
