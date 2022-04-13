.extern _Ravef4main

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
	movq $0x2000001, %rax
	syscall

.global _Ravef7_fwrite
.type _Ravef7_fwrite, @function
_Ravef7_fwrite:
	movq $0x20000004, %rax
	syscall
	ret

.global _Ravef5fread
.type   _Ravef5fread, @function
_Ravef5fread:
	movq $0x20000003, %rax
	syscall
	ret

.global _start
.type   _start, @function
_start:
	call _Ravef4main
	movq %rax, %rdi
	movq $0x2000001, %rax
	syscall
	