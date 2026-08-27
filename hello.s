	.file	"hello.c"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movabsq	$7596482334399685960, %rax
	movq	%rax, -14(%rbp)
	movabsq	$2851551017068908, %rax
	movq	%rax, -8(%rbp)
	leaq	-14(%rbp), %rax
	movl	$13, %edx
	movq	%rax, %rsi
	movl	$1, %edi
	call	write
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (GNU) 16.2.1 20260819 (Red Hat 16.2.1-2)"
	.section	.note.GNU-stack,"",@progbits
