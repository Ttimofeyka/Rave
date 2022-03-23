	.text
	.file	"rave"
	.globl	_Ravef4main             # -- Begin function _Ravef4main
	.p2align	4, 0x90
	.type	_Ravef4main,@function
_Ravef4main:                            # @_Ravef4main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	$6, %edi
	movl	$3, %esi
	movl	$1, %edx
	movl	$8, %ecx
	movl	$9, %r8d
	callq	_Ravef2sd
	xorl	%eax, %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	_Ravef4main, .Lfunc_end0-_Ravef4main
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
