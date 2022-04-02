.extern _Ravef4main

.global _Ravef4exit
.type	_Ravef4exit, @function
_Ravef4exit:
	movl $1, %eax
	int $0x80

.global _Ravef5write
.type    _Ravef5write, @function
_Ravef5write:
	movl $4, %eax
	int $0x80
	ret
	
.global _Ravef4read
.type   _Ravef4read, @function
_Ravef4read:
	movl $3, %eax
	int $0x80
	ret

.global _start
.type	_start, @function
_start:
	call _Ravef4main

   	movl %eax, %ebx
	call _Ravef4exit
