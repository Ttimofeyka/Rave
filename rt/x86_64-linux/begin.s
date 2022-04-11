.extern _Ravef4main

.global _start
.type   _start, @function
_start:
	movq (%rsp), %rdi
	lea 8(%rsp), %rsi
	call _Ravef4main

	movq %rax, %rdi
	movq $60, %rax
  	syscall
	  