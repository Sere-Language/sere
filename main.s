	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
@feat.00 = 0
	.file	"main.sere"
	.def	Exception___init__;
	.scl	2;
	.type	32;
	.endef
	.text
	.globl	Exception___init__              # -- Begin function Exception___init__
	.p2align	4
Exception___init__:                     # @Exception___init__
.seh_proc Exception___init__
# %bb.0:                                # %entry
	subq	$16, %rsp
	.seh_stackalloc 16
	.seh_endprologue
	movq	%rdx, (%rsp)
	movq	%r8, 8(%rsp)
	movq	(%rsp), %rax
	movq	8(%rsp), %rdx
	movq	%rdx, 16(%rcx)
	movq	%rax, 8(%rcx)
	.seh_startepilogue
	addq	$16, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	sere_main;
	.scl	2;
	.type	32;
	.endef
	.globl	sere_main                       # -- Begin function sere_main
	.p2align	4
sere_main:                              # @sere_main
.seh_proc sere_main
# %bb.0:                                # %entry
	subq	$40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	callq	sere.module.init
	leaq	.L__unnamed_1(%rip), %rcx
	movl	$13, %edx
	callq	sere_write
	callq	sere_write_nl
	xorl	%eax, %eax
	movl	%eax, %edx
	movl	$1, %eax
	.seh_startepilogue
	addq	$40, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	sere.module.init;
	.scl	3;
	.type	32;
	.endef
	.p2align	4                               # -- Begin function sere.module.init
sere.module.init:                       # @sere.module.init
# %bb.0:                                # %entry
	cmpb	$0, sere.module.init.done(%rip)
	jne	.LBB2_2
# %bb.1:                                # %init
	movb	$1, sere.module.init.done(%rip)
.LBB2_2:                                # %end
	retq
                                        # -- End function
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	callq	sere_mod_init
	callq	sere.module.init
	callq	sere_main
	xorl	%eax, %eax
	.seh_startepilogue
	addq	$40, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	sere_mod_init;
	.scl	2;
	.type	32;
	.endef
	.weak	sere_mod_init                   # -- Begin function sere_mod_init
	.p2align	4
sere_mod_init:                          # @sere_mod_init
# %bb.0:                                # %entry
	retq
                                        # -- End function
	.lcomm	sere.module.init.done,1         # @sere.module.init.done
	.section	.rdata,"dr"
.L__unnamed_1:                          # @0
	.asciz	"Hello, World!"

	.addrsig
	.addrsig_sym sere_main
	.addrsig_sym sere.module.init
	.addrsig_sym sere_write
	.addrsig_sym sere_write_nl
	.addrsig_sym sere_mod_init
	.addrsig_sym sere.module.init.done
