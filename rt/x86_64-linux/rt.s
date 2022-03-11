.extern _Ravef4main

.global _Ravef4exit
.type   _Ravef4exit, @function
_Ravef4exit:
 	movq $60, %rax
  	syscall

.global _start
.type   _start, @function
_start:
	call _Ravef4main
	movq %rax, %rdi
	movq $60, %rax
	syscall
