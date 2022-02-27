.extern main

.org 0
rjmp _start

.global _EPLf4exit
.type	_EPLf4exit, @function
_EPLf4exit:
    rjmp _EPLf4exit
    ret

.global _start
.type	_start, @function
_start:
    rcall main
    ret
