.global _Ravef5sysc0
.type _Ravef5sysc, @function
_Ravef4sysc:
	movsx %edi, %rax
	syscall
	ret

.global _Ravef5sysc1
.type   _Ravef5sysc1, @function
_Ravef5sysc1:
	movsx %edi, %rax
	movsx %esi, %rdi
	syscall
	ret

.global _Ravef5sysc2
.type   _Ravef5sysc2, @function
_Ravef5sysc2:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	syscall
	ret

.global _Ravef5sysc3
.type   _Ravef5sysc3, @function
_Ravef5sysc3:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	syscall
	ret

.global _Ravef5sysc4
.type   _Ravef5sysc4, @function
_Ravef5sysc4:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	syscall
	ret

.global _Ravef5sysc5
.type   _Ravef5sysc5, @function
_Ravef5sysc5:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	movsx %r9d, %r8
	syscall
	ret
.global _Ravef5sysc6
.type   _Ravef5sysc6, @function
_Ravef5sysc6:
	movsx %edi, %rax
	movsx %esi, %rdi
	movsx %edx, %rsi
	movsx %ecx, %rdx
	movsx %r8d, %r10
	movsx %r9d, %r8
	movsx 8(%rsp), %r9
	syscall
	ret
