.extern _Ravef4main

.global "_Ravef10std::sysc0"
.type   "_Ravef10std::sysc0", @function
"_Ravef10std::sysc0":
	movsx %edi, %rax
	syscall
	ret

.global "_Ravef10std::sysc1"
.type   "_Ravef10std::sysc1", @function
"_Ravef10std::sysc1":
	movsx %edi, %rax
	movsx %esi, %rdi
	syscall
	ret

.global _start
.type   _start, @function
_start:
	movq (%rsp), %rdi
	lea 8(%rsp), %rsi
	call _Ravef4main

	movq %rax, %rdi
	movq $60, %rax
  	syscall
	  