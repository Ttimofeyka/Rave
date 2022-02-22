.extern main

.global _start
_start:
    call main
    movl $1, %eax
    movl $0, %ebx
    int $0x80
