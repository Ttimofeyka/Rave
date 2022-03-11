.extern _Ravef4main

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
    movl $1, %eax
    int $0x80

.global write
write:
	movl $4, %eax
	movl $1, %ebx
	movl %ebx, %ecx
	movl $0, %edx

	int $0x80
	ret

.global _start
.type   _start, @function
_start:
    call _Ravef4main
    movl %eax, %ebx
    movl $1, %eax
    int $0x80
