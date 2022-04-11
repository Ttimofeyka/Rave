.global _Ravef4exit
.type	_Ravef4exit, @function
_Ravef4exit:
	movl $1, %eax
	int $0x80

.global _Ravef7_fwrite
.type   _Ravef7_fwrite, @function
_Ravef7_fwrite:
	movl $4, %eax
	int $0x80
	ret
	
.global _Ravef5fread
.type   _Ravef5fread, @function
_Ravef5fread:
	movl $3, %eax
	int $0x80
	ret

.global _Ravef5fopen
.type   _Ravef5fopen, @function
_Ravef5fopen:
	movl $5, %eax
	int $0x80
	ret

.global _Ravef6fclose
.type   _Ravef6fclose, @function
_Ravef6fclose:
	movl $6, %eax
	int $0x80
	ret

.global _Ravef5mkdir
.type   _Ravef5mkdir, @function
_Ravef5mkdir:
	movl $39, %eax
	int $0x80
	ret

.global _Ravef5rmdir
.type   _Ravef5rmdir, @function
_Ravef5rmdir:
	movl $40, %eax
	int $0x80
	ret

.global _Ravef6uselib
.type   _Ravef6uselib, @function
_Ravef6uselib:
	movl $86, %eax
	int $0x80
	ret

.global _Ravef6fchmod
.type   _Ravef6fchmod, @function
_Ravef6fchmod:
	movl $94, %eax
	int $0x80
	ret

.global _Ravef3brk
.type   _Ravef3brk, @function
_Ravef3brk:
	movl $45, %eax
	int $0x80
	ret
	