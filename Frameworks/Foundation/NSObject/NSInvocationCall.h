/*
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
 */

// The register and stack image NSInvocationCall-x86_64.s loads before
// calling an IMP, and the return registers it stores after.

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t gpr[6];            // rdi rsi rdx rcx r8 r9
    uint64_t xmm[8];            // xmm0-7 (low 8 bytes)
    uint64_t *stack;            // words pushed in order, first at the lowest address
    uint64_t stackCount;
    uint64_t rax, rdx;          // returned
    uint64_t xmm0, xmm1;        // returned
} NSInvocationCallFrame;

// The offsets the assembly uses.
_Static_assert(offsetof(NSInvocationCallFrame, xmm) == 48, "xmm");
_Static_assert(offsetof(NSInvocationCallFrame, stack) == 112, "stack");
_Static_assert(offsetof(NSInvocationCallFrame, stackCount) == 120, "stackCount");
_Static_assert(offsetof(NSInvocationCallFrame, rax) == 128, "rax");
_Static_assert(offsetof(NSInvocationCallFrame, xmm0) == 144, "xmm0");

void NSInvocationCall(void *imp, NSInvocationCallFrame *frame);
