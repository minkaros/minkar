/*
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * void NSInvocationCall(IMP imp, NSInvocationCallFrame *frame)
 *
 * Calls imp with the System V x86-64 calling convention: the six integer
 * argument registers, the eight vector ones and the stack words in frame
 * (NSInvocationCall.h has the layout), then stores the two integer and two
 * vector return registers back into frame.
 */

	.text
	.globl	_NSInvocationCall
	.p2align 4
_NSInvocationCall:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%rbx	/* callee-saved: the frame */
	pushq	%r12	/* callee-saved: the IMP */
	.cfi_offset %rbx, -24
	.cfi_offset %r12, -32
	movq	%rsi, %rbx
	movq	%rdi, %r12

/* Room for the stack words, keeping %rsp 16-byte aligned at the call. */
	movq	120(%rbx), %rcx	/* stackCount */
	leaq	15(,%rcx,8), %rax
	andq	$-16, %rax
	subq	%rax, %rsp
	movq	112(%rbx), %rsi	/* stack */
	xorl	%edx, %edx
1:	cmpq	%rcx, %rdx
	jae	2f
	movq	(%rsi,%rdx,8), %rax
	movq	%rax, (%rsp,%rdx,8)
	incq	%rdx
	jmp	1b

2:	movsd	48(%rbx), %xmm0
	movsd	56(%rbx), %xmm1
	movsd	64(%rbx), %xmm2
	movsd	72(%rbx), %xmm3
	movsd	80(%rbx), %xmm4
	movsd	88(%rbx), %xmm5
	movsd	96(%rbx), %xmm6
	movsd	104(%rbx), %xmm7
	movq	0(%rbx), %rdi
	movq	8(%rbx), %rsi
	movq	16(%rbx), %rdx
	movq	24(%rbx), %rcx
	movq	32(%rbx), %r8
	movq	40(%rbx), %r9
	movl	$8, %eax	/* vector registers used, for variadic callees */
	callq	*%r12

	movq	%rax, 128(%rbx)
	movq	%rdx, 136(%rbx)
	movsd	%xmm0, 144(%rbx)
	movsd	%xmm1, 152(%rbx)

	leaq	-16(%rbp), %rsp
	popq	%r12
	popq	%rbx
	popq	%rbp
	retq
	.cfi_endproc

/*
 * The forward handlers (objc_setForwardHandler): _objc_msgForward jumps
 * here, with the message's registers and stack as the caller set them.
 *
 * The registers are saved in a frame: rdi..r9 at 0..40, xmm0-7 at 48..104,
 * the return registers at 112..136. First NSInvocationFastTarget may name
 * another receiver (forwardingTargetForSelector:): then the registers are
 * put back, the receiver replaced, and the message sent to it. Otherwise
 * NSInvocationForwardRegisters makes an NSInvocation from the frame and the
 * caller's stack arguments, calls -forwardInvocation:, and leaves the
 * return value in the frame, from where it is loaded and returned.
 */
	.globl	_NSInvocationForward
	.globl	_NSInvocationForward_stret
	.p2align 4
_NSInvocationForward_stret:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$176, %rsp
	movq	$1, 144(%rsp)		/* struct return: rdi is the buffer */
	jmp	Lsave
	.cfi_endproc

/*
 * The unwind information (.cfi) lets an exception raised while forwarding
 * (-forwardInvocation:, -doesNotRecognizeSelector:) reach the caller's
 * handler; both entries build the same frame before Lsave.
 */
	.p2align 4
_NSInvocationForward:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$176, %rsp
	movq	$0, 144(%rsp)		/* not a struct return */

Lsave:	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	%rdx, 16(%rsp)
	movq	%rcx, 24(%rsp)
	movq	%r8, 32(%rsp)
	movq	%r9, 40(%rsp)
	movsd	%xmm0, 48(%rsp)
	movsd	%xmm1, 56(%rsp)
	movsd	%xmm2, 64(%rsp)
	movsd	%xmm3, 72(%rsp)
	movsd	%xmm4, 80(%rsp)
	movsd	%xmm5, 88(%rsp)
	movsd	%xmm6, 96(%rsp)
	movsd	%xmm7, 104(%rsp)

	movq	%rsp, %rdi
	movq	144(%rsp), %rsi
	callq	_NSInvocationFastTarget
	testq	%rax, %rax
	jz	Lslow

	/* Another receiver: send it the message as it came. */
	cmpq	$0, 144(%rsp)
	jne	1f
	movq	%rax, 0(%rsp)
	jmp	2f
1:	movq	%rax, 8(%rsp)
2:	movq	0(%rsp), %rdi
	movq	8(%rsp), %rsi
	movq	16(%rsp), %rdx
	movq	24(%rsp), %rcx
	movq	32(%rsp), %r8
	movq	40(%rsp), %r9
	movsd	48(%rsp), %xmm0
	movsd	56(%rsp), %xmm1
	movsd	64(%rsp), %xmm2
	movsd	72(%rsp), %xmm3
	movsd	80(%rsp), %xmm4
	movsd	88(%rsp), %xmm5
	movsd	96(%rsp), %xmm6
	movsd	104(%rsp), %xmm7
	movq	144(%rsp), %rax
	movq	%rbp, %rsp
	popq	%rbp
	testq	%rax, %rax
	jnz	3f
	movl	$8, %eax
	jmp	_objc_msgSend
3:	movl	$8, %eax
	jmp	_objc_msgSend_stret

Lslow:	movq	%rsp, %rdi		/* the saved registers */
	leaq	16(%rbp), %rsi		/* the caller's stack arguments */
	leaq	112(%rsp), %rdx		/* the return registers */
	movq	144(%rsp), %rcx		/* struct return? */
	callq	_NSInvocationForwardRegisters
	movq	112(%rsp), %rax
	movq	120(%rsp), %rdx
	movsd	128(%rsp), %xmm0
	movsd	136(%rsp), %xmm1
	movq	%rbp, %rsp
	popq	%rbp
	retq
	.cfi_endproc

/*
 * The invoke function of blocks made at run time (NSInvocation.m has
 * NSInvocationBlockForwardRegisters): the block is in rdi. The registers
 * are saved in a frame laid out as above, and the return value is loaded
 * from it.
 */
	.globl	_NSInvocationBlockForward
	.p2align 4
_NSInvocationBlockForward:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$176, %rsp
	movq	%rdi, 0(%rsp)
	movq	%rsi, 8(%rsp)
	movq	%rdx, 16(%rsp)
	movq	%rcx, 24(%rsp)
	movq	%r8, 32(%rsp)
	movq	%r9, 40(%rsp)
	movsd	%xmm0, 48(%rsp)
	movsd	%xmm1, 56(%rsp)
	movsd	%xmm2, 64(%rsp)
	movsd	%xmm3, 72(%rsp)
	movsd	%xmm4, 80(%rsp)
	movsd	%xmm5, 88(%rsp)
	movsd	%xmm6, 96(%rsp)
	movsd	%xmm7, 104(%rsp)
	movq	%rsp, %rdi		/* the saved registers */
	leaq	16(%rbp), %rsi		/* the caller's stack arguments */
	leaq	112(%rsp), %rdx		/* the return registers */
	callq	_NSInvocationBlockForwardRegisters
	movq	112(%rsp), %rax
	movq	120(%rsp), %rdx
	movsd	128(%rsp), %xmm0
	movsd	136(%rsp), %xmm1
	movq	%rbp, %rsp
	popq	%rbp
	retq
	.cfi_endproc
