.extern main

.org 0
rjmp _start

.global _start
_start:
    rcall main
    ret
