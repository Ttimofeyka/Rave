.extern main

.global exit
exit:
 	movq $60, %rax
  	syscall

.global write
write:
	push %rdi
	movq     $1, %rax    # sys_write call number 
	movq     $1, %rdi    # write to stdout (fd=1)
	movq     %rsp, %rsi  # use char on stack
	movq     $1, %rdx    # write 1 char
	syscall   
	pop %rdi
	ret

.global _start
_start:
	call main
	movq $0x21,%rdi
	call write
	movq %rax, %rdi
	movq $60, %rax
	syscall
	