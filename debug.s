	.text
	.file	"rave"
	.globl	_Ravef1q                # -- Begin function _Ravef1q
	.p2align	4, 0x90
	.type	_Ravef1q,@function
_Ravef1q:                               # @_Ravef1q
	.cfi_startproc
# %bb.0:                                # %entry
	incl	%edi
	movl	%edi, -4(%rsp)
	incl	%esi
	movl	%esi, -8(%rsp)
	incl	%edx
	movl	%edx, -12(%rsp)
	incl	%ecx
	movl	%ecx, -16(%rsp)
	incl	%r8d
	movl	%r8d, -20(%rsp)
	incl	%r9d
	movl	%r9d, -24(%rsp)
	incl	8(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end0:
	.size	_Ravef1q, .Lfunc_end0-_Ravef1q
	.cfi_endproc
                                        # -- End function
	.globl	_Ravef4main             # -- Begin function _Ravef4main
	.p2align	4, 0x90
	.type	_Ravef4main,@function
_Ravef4main:                            # @_Ravef4main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	$0, (%rsp)
	xorl	%edi, %edi
	xorl	%esi, %esi
	xorl	%edx, %edx
	xorl	%ecx, %ecx
	xorl	%r8d, %r8d
	xorl	%r9d, %r9d
	callq	_Ravef1q
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	_Ravef4main, .Lfunc_end1-_Ravef4main
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
