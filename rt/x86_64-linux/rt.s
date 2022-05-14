.global _Ravef15_nsstdfuncsysc0
.type   _Ravef15_nsstdfuncsysc0, @function
_Ravef15_nsstdfuncsysc0:
	movsx %edi, %rax
	syscall
	ret

.global _Ravef15_nsstdfuncsysc1
.type   _Ravef15_nsstdfuncsysc1, @function
_Ravef15_nsstdfuncsysc1:
	movsx %edi, %rax
	movsx %esi, %rdi
	syscall
	ret

.global _Ravef15_nsstdfuncsysc2
.type   _Ravef15_nsstdfuncsysc2, @function
_Ravef15_nsstdfuncsysc2:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	syscall
	ret

.global _Ravef15_nsstdfuncsysc3
.type   _Ravef15_nsstdfuncsysc3, @function
_Ravef15_nsstdfuncsysc3:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	syscall
	ret

.global _Ravef15_nsstdfuncsysc4
.type   _Ravef15_nsstdfuncsysc4, @function
_Ravef15_nsstdfuncsysc4:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	syscall
	ret

.global _Ravef15_nsstdfuncsysc5
.type   _Ravef15_nsstdfuncsysc5, @function
_Ravef15_nsstdfuncsysc5:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	movsx %r9d, %r8
	syscall
	ret
.global _Ravef15_nsstdfuncsysc6
.type   _Ravef15_nsstdfuncsysc6, @function
_Ravef15_nsstdfuncsysc6:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	movsx %r9d, %r8
	movsx 8(%rsp), %r9
	syscall
	ret
