.global _Ravef5sysc0
.type _Ravef5sysc, @function
_Ravef4sysc:
	movl %edi, %eax
	syscall
	ret

.global _Ravef5sysc1
.type   _Ravef5sysc1, @function
_Ravef5sysc1:
	movl %edi, %eax
	movl %esi, %ebx
	syscall
	ret

.global _Ravef5sysc2
.type   _Ravef5sysc2, @function
_Ravef5sysc2:
	movl %edi, %eax
	movl %esi, %ebx
	movl %edx, %ecx
	syscall
	ret

.global _Ravef5sysc3
.type   _Ravef5sysc3, @function
_Ravef5sysc3:
	movl %edi, %eax
	movl %esi, %ebx
	movl %edx, %ecx
	movl %ecx, %esi
	syscall
	ret
	