.extern main

.org 0
rjmp _start

.global exit
.type	exit, @function
exit:
    rjmp exit
    ret

.global _starts
_start:
    rcall main
    ret
