.extern _Ravef4main

.org 0
rjmp _start

.global _Ravef4exit
.type	_Ravef4exit, @function
_Ravef4exit:
    rjmp _Ravef4exit
    ret

.global _start
.type	_start, @function
_start:
    rcall _Ravef4main
    ret
