.extern _Ravef4main

.global _start
.type   _start, @function
_start:
	movl (%esp), %edi
	lea 8(%esp), %esi
	call _Ravef4main

	movl %eax, %edi
	movl $60, %eax
  	int $0x80
	  