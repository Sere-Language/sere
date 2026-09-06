	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
@feat.00 = 0
	.file	"main.sere"
	.def	abs;
	.scl	2;
	.type	32;
	.endef
	.text
	.globl	abs                             # -- Begin function abs
	.p2align	4
abs:                                    # @abs
.seh_proc abs
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movl	%ecx, 4(%rsp)
	cmpl	$0, 4(%rsp)
	jl	.LBB0_2
	jmp	.LBB0_3
.LBB0_1:                                # %if.end
	movl	4(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB0_2:                                # %if.then
	xorl	%eax, %eax
	subl	4(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB0_3:                                # %if.next
	jmp	.LBB0_1
	.seh_endproc
                                        # -- End function
	.def	min;
	.scl	2;
	.type	32;
	.endef
	.globl	min                             # -- Begin function min
	.p2align	4
min:                                    # @min
.seh_proc min
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movl	%ecx, 4(%rsp)
	movl	%edx, (%rsp)
	movl	4(%rsp), %eax
	cmpl	(%rsp), %eax
	jge	.LBB1_2
# %bb.1:                                # %if.then
	movl	4(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB1_2:                                # %if.next
	movl	(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	max;
	.scl	2;
	.type	32;
	.endef
	.globl	max                             # -- Begin function max
	.p2align	4
max:                                    # @max
.seh_proc max
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movl	%ecx, 4(%rsp)
	movl	%edx, (%rsp)
	movl	4(%rsp), %eax
	cmpl	(%rsp), %eax
	jle	.LBB2_2
# %bb.1:                                # %if.then
	movl	4(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB2_2:                                # %if.next
	movl	(%rsp), %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	clamp;
	.scl	2;
	.type	32;
	.endef
	.globl	clamp                           # -- Begin function clamp
	.p2align	4
clamp:                                  # @clamp
.seh_proc clamp
# %bb.0:                                # %entry
	subq	$56, %rsp
	.seh_stackalloc 56
	.seh_endprologue
	movl	%ecx, 52(%rsp)
	movl	%edx, 48(%rsp)
	movl	%r8d, 44(%rsp)
	movl	52(%rsp), %ecx
	movl	48(%rsp), %edx
	callq	max
	movl	%eax, %ecx
	movl	44(%rsp), %edx
	callq	min
	nop
	.seh_startepilogue
	addq	$56, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	sign;
	.scl	2;
	.type	32;
	.endef
	.globl	sign                            # -- Begin function sign
	.p2align	4
sign:                                   # @sign
.seh_proc sign
# %bb.0:                                # %entry
	pushq	%rax
	.seh_stackalloc 8
	.seh_endprologue
	movl	%ecx, 4(%rsp)
	cmpl	$0, 4(%rsp)
	jg	.LBB4_2
	jmp	.LBB4_3
.LBB4_1:                                # %if.end
	xorl	%eax, %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB4_2:                                # %if.then
	movl	$1, %eax
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB4_3:                                # %if.next
	cmpl	$0, 4(%rsp)
	jge	.LBB4_5
# %bb.4:                                # %if.then2
	movl	$4294967295, %eax               # imm = 0xFFFFFFFF
	.seh_startepilogue
	popq	%rcx
	.seh_endepilogue
	retq
.LBB4_5:                                # %if.next3
	jmp	.LBB4_1
	.seh_endproc
                                        # -- End function
	.def	Exception___init__;
	.scl	2;
	.type	32;
	.endef
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
	.def	Animal___init__;
	.scl	2;
	.type	32;
	.endef
	.globl	Animal___init__                 # -- Begin function Animal___init__
	.p2align	4
Animal___init__:                        # @Animal___init__
.seh_proc Animal___init__
# %bb.0:                                # %entry
	subq	$32, %rsp
	.seh_stackalloc 32
	.seh_endprologue
	movq	72(%rsp), %rax
	movq	%rdx, 16(%rsp)
	movq	%r8, 24(%rsp)
	movq	%rax, 8(%rsp)
	movq	%r9, (%rsp)
	movq	16(%rsp), %rax
	movq	24(%rsp), %rdx
	movq	%rdx, 16(%rcx)
	movq	%rax, 8(%rcx)
	movq	(%rsp), %rax
	movq	8(%rsp), %rdx
	movq	%rdx, 32(%rcx)
	movq	%rax, 24(%rcx)
	.seh_startepilogue
	addq	$32, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	Animal___str__;
	.scl	2;
	.type	32;
	.endef
	.globl	Animal___str__                  # -- Begin function Animal___str__
	.p2align	4
Animal___str__:                         # @Animal___str__
.seh_proc Animal___str__
# %bb.0:                                # %entry
	subq	$72, %rsp
	.seh_stackalloc 72
	.seh_endprologue
	movq	%rcx, %rax
	movq	%rax, 40(%rsp)                  # 8-byte Spill
	movq	8(%rax), %rcx
	movq	16(%rax), %rdx
	movq	%rsp, %rax
	leaq	64(%rsp), %r8
	movq	%r8, 32(%rax)
	leaq	.L__unnamed_1(%rip), %r8
	movl	$2, %r9d
	callq	sere_str_concat_data
	movq	%rax, %rcx
	movq	40(%rsp), %rax                  # 8-byte Reload
	movq	64(%rsp), %rdx
	movq	24(%rax), %r8
	movq	32(%rax), %r9
	movq	%rsp, %rax
	leaq	56(%rsp), %r10
	movq	%r10, 32(%rax)
	callq	sere_str_concat_data
	movq	%rax, %rcx
	movq	56(%rsp), %rdx
	movq	%rsp, %rax
	leaq	48(%rsp), %r8
	movq	%r8, 32(%rax)
	leaq	.L__unnamed_2(%rip), %r8
	movl	$1, %r9d
	callq	sere_str_concat_data
	movq	48(%rsp), %rdx
	.seh_startepilogue
	addq	$72, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	Dog___init__;
	.scl	2;
	.type	32;
	.endef
	.globl	Dog___init__                    # -- Begin function Dog___init__
	.p2align	4
Dog___init__:                           # @Dog___init__
.seh_proc Dog___init__
# %bb.0:                                # %entry
	subq	$88, %rsp
	.seh_stackalloc 88
	.seh_endprologue
	movq	%rcx, 48(%rsp)                  # 8-byte Spill
	movq	128(%rsp), %rax
	movq	%rdx, 72(%rsp)
	movq	%r8, 80(%rsp)
	movq	%rax, 64(%rsp)
	movq	%r9, 56(%rsp)
	movq	72(%rsp), %rdx
	movq	80(%rsp), %r8
	movq	%rsp, %rax
	movq	$3, 32(%rax)
	leaq	.L__unnamed_3(%rip), %r9
	callq	Animal___init__
	movq	48(%rsp), %rcx                  # 8-byte Reload
	movq	56(%rsp), %rax
	movq	64(%rsp), %rdx
	movq	%rdx, 48(%rcx)
	movq	%rax, 40(%rcx)
	.seh_startepilogue
	addq	$88, %rsp
	.seh_endepilogue
	retq
	.seh_endproc
                                        # -- End function
	.def	Dog_bark;
	.scl	2;
	.type	32;
	.endef
	.globl	Dog_bark                        # -- Begin function Dog_bark
	.p2align	4
Dog_bark:                               # @Dog_bark
# %bb.0:                                # %entry
	leaq	.L__unnamed_4(%rip), %rax
	movl	$5, %edx
	retq
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
	subq	$152, %rsp
	.seh_stackalloc 152
	.seh_endprologue
	callq	sere.module.init
	movq	$0, 88(%rsp)
	movq	$0, 80(%rsp)
	movq	$0, 72(%rsp)
	movq	$0, 64(%rsp)
	movq	$0, 56(%rsp)
	movq	$0, 48(%rsp)
	movl	$0, 40(%rsp)
	movl	$1265483177, 40(%rsp)           # imm = 0x4B6DBDA9
	movq	%rsp, %rax
	movq	$16, 32(%rax)
	leaq	.L__unnamed_5(%rip), %rdx
	leaq	.L__unnamed_6(%rip), %r9
	leaq	40(%rsp), %rcx
	movl	$5, %r8d
	callq	Dog___init__
	movl	40(%rsp), %eax
	movq	48(%rsp), %rcx
	movq	56(%rsp), %rdx
	movq	64(%rsp), %r8
	movq	72(%rsp), %r9
	movq	80(%rsp), %r10
	movq	88(%rsp), %r11
	movq	%r11, 144(%rsp)
	movq	%r10, 136(%rsp)
	movq	%r9, 128(%rsp)
	movq	%r8, 120(%rsp)
	movq	%rdx, 112(%rsp)
	movq	%rcx, 104(%rsp)
	movl	%eax, 96(%rsp)
	movl	$1, %ecx
	callq	sere_write_bool
	callq	sere_write_nl
	leaq	96(%rsp), %rcx
	callq	Dog_bark
	movq	%rax, %rcx
	callq	sere_write
	callq	sere_write_nl
	xorl	%eax, %eax
	.seh_startepilogue
	addq	$152, %rsp
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
	jne	.LBB11_2
# %bb.1:                                # %init
	movb	$1, sere.module.init.done(%rip)
.LBB11_2:                               # %end
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
	nop
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
	.asciz	" ("

.L__unnamed_2:                          # @1
	.asciz	")"

.L__unnamed_3:                          # @2
	.asciz	"Dog"

.L__unnamed_4:                          # @3
	.asciz	"Woof!"

.L__unnamed_5:                          # @4
	.asciz	"Buddy"

.L__unnamed_6:                          # @5
	.asciz	"Golden Retriever"

.L__unnamed_7:                          # @6
	.asciz	" "

.L__unnamed_8:                          # @7
	.asciz	" "

	.addrsig
	.addrsig_sym min
	.addrsig_sym max
	.addrsig_sym Animal___init__
	.addrsig_sym Dog___init__
	.addrsig_sym Dog_bark
	.addrsig_sym sere_main
	.addrsig_sym sere.module.init
	.addrsig_sym sere_str_concat_data
	.addrsig_sym sere_write_nl
	.addrsig_sym sere_write_bool
	.addrsig_sym sere_write
	.addrsig_sym sere_mod_init
	.addrsig_sym sere.module.init.done
