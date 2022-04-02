.extern _Ravef4main

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
 	movq $60, %rax
  	syscall

.global _Ravef7_fwrite
.type   _Ravef7_fwrite, @function
_Ravef7_fwrite:
	movq $1, %rax
	syscall
	ret
	
.global _Ravef4read
.type   _Ravef4read, @function
_Ravef4read:
	movq $0, %rax
	syscall
	ret

.global _Ravef5fopen
.type   _Ravef5fopen, @function
_Ravef5fopen:
	movq $2, %rax
	syscall
	ret

.global _Ravef6fclose
.type   _Ravef6fclose, @function
_Ravef6fclose:
	movq $3, %rax
	syscall
	ret

.global _start
.type   _start, @function
_start:
	call _Ravef4main

	movq %rax, %rdi
	call _Ravef4exit
