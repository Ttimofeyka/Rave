.global _Ravef15_nsstdfuncsysc0
.type _Ravef15_nsstdfuncsysc, @function
_Ravef4sysc:
	movl %edi, %eax
	syscall
	ret

.global _Ravef15_nsstdfuncsysc1
.type   _Ravef15_nsstdfuncsysc1, @function
_Ravef15_nsstdfuncsysc1:
	movl %edi, %eax
	movl %esi, %ebx
	syscall
	ret

.global _Ravef15_nsstdfuncsysc2
.type   _Ravef15_nsstdfuncsysc2, @function
_Ravef15_nsstdfuncsysc2:
	movl %edi, %eax
	movl %esi, %ebx
	movl %edx, %ecx
	syscall
	ret

.global _Ravef15_nsstdfuncsysc3
.type   _Ravef15_nsstdfuncsysc3, @function
_Ravef15_nsstdfuncsysc3:
	movl %edi, %eax
	movl %esi, %ebx
	movl %edx, %ecx
	movl %ecx, %esi
	syscall
	ret
	