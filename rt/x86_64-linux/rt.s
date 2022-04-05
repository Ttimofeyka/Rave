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
	
.global _Ravef5fread
.type   _Ravef5fread, @function
_Ravef5fread:
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

.global _Ravef5mkdir
.type   _Ravef5mkdir, @function
_Ravef5mkdir:
	movq $83, %rax
	syscall
	ret

.global _Ravef5rmdir
.type   _Ravef5rmdir, @function
_Ravef5rmdir:
	movq $84, %rax
	syscall
	ret

.global _Ravef5chmod
.type   _Ravef5chmod, @function
_Ravef5chmod:
	movq $90, %rax
	syscall
	ret

.global _Ravef6uselib
.type   _Ravef6uselib, @function
_Ravef6uselib:
	movq $134, %rax
	syscall
	ret

.global _Ravef6fchmod
.type   _Ravef6fchmod, @function
_Ravef6fchmod:
	movq $91, %rax
	syscall
	ret

.global _Ravef3brk
.type   _Ravef3brk, @function
_Ravef3brk:
	movq $12, %rax
	syscall
	ret

.global _start
.type   _start, @function
_start:
	call _Ravef4main

	movq %rax, %rdi
	call _Ravef4exit
