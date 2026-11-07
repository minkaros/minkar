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

/*
 * C99's floating point environment with macOS's fenv_t layout (<fenv.h>):
 * every function acts on the x87 unit and SSE (MXCSR) together, as one
 * environment. Replaces msun's fenv.c, whose fenv_t is FreeBSD's. Written
 * for Minkar.
 */

/* Minkar's <fenv.h> (msun's comes first on the include path here). */
#include "../../../Developer/Minkar.sdk/usr/include/fenv.h"
#include <stdint.h>

/* MXCSR: exception flags in bits 0-5, their masks in bits 7-12 (the same
 * order), rounding in bits 13-14 (the x87 control word's, shifted by 3),
 * DAZ (bit 6) and FTZ (bit 15). */
#define	MXCSR_MASK_SHIFT	7
#define	MXCSR_ROUND_SHIFT	3
#define	MXCSR_DAZ		0x0040
#define	MXCSR_FTZ		0x8000

const fenv_t _FE_DFL_ENV = { 0x037f, 0x0000, 0x1f80, { 0 } };
const fenv_t _FE_DFL_DISABLE_SSE_DENORMS_ENV = { 0x037f, 0x0000,
    0x1f80 | MXCSR_DAZ | MXCSR_FTZ, { 0 } };

/* The x87 environment as fnstenv stores it in 32-bit protected mode: the
 * control, status and tag words, each in 32 bits, then the pointers. */
struct x87env {
	uint32_t control, status, tag, other[4];
};

static inline void
x87_get(struct x87env *e)
{
	__asm__ __volatile__("fnstenv %0" : "=m" (*e));
	/* fnstenv masks every exception; the control word goes back as it was. */
	__asm__ __volatile__("fldcw %0" : : "m" (e->control));
}

static inline void
x87_set(const struct x87env *e)
{
	__asm__ __volatile__("fldenv %0" : : "m" (*e));
}

static inline uint32_t
mxcsr_get(void)
{
	uint32_t v;
	__asm__ __volatile__("stmxcsr %0" : "=m" (v));
	return v;
}

static inline void
mxcsr_set(uint32_t v)
{
	__asm__ __volatile__("ldmxcsr %0" : : "m" (v));
}

int
feclearexcept(int excepts)
{
	struct x87env e;
	excepts &= FE_ALL_EXCEPT;
	x87_get(&e);
	e.status &= ~(uint32_t)excepts;
	x87_set(&e);
	mxcsr_set(mxcsr_get() & ~(uint32_t)excepts);
	return 0;
}

int
fetestexcept(int excepts)
{
	uint16_t status;
	__asm__ __volatile__("fnstsw %0" : "=m" (status));
	return (int)((status | mxcsr_get()) & (uint32_t)excepts & FE_ALL_EXCEPT);
}

int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	*flagp = (fexcept_t)fetestexcept(excepts);
	return 0;
}

/* The flags set without trapping: in the status word and MXCSR. */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	struct x87env e;
	excepts &= FE_ALL_EXCEPT;
	x87_get(&e);
	e.status = (e.status & ~(uint32_t)excepts) | (*flagp & (uint32_t)excepts);
	x87_set(&e);
	mxcsr_set((mxcsr_get() & ~(uint32_t)excepts) | (*flagp & (uint32_t)excepts));
	return 0;
}

/* Raised as an operation would: an unmasked one traps at the next x87
 * instruction (fwait). */
int
feraiseexcept(int excepts)
{
	struct x87env e;
	excepts &= FE_ALL_EXCEPT;
	x87_get(&e);
	e.status |= (uint32_t)excepts;
	x87_set(&e);
	__asm__ __volatile__("fwait");
	return 0;
}

int
fegetround(void)
{
	uint16_t control;
	__asm__ __volatile__("fnstcw %0" : "=m" (control));
	return control & FE_TOWARDZERO;
}

int
fesetround(int round)
{
	uint16_t control;
	if ((round & ~FE_TOWARDZERO) != 0)
		return -1;
	__asm__ __volatile__("fnstcw %0" : "=m" (control));
	control = (uint16_t)((control & ~FE_TOWARDZERO) | round);
	__asm__ __volatile__("fldcw %0" : : "m" (control));
	mxcsr_set((mxcsr_get() & ~((uint32_t)FE_TOWARDZERO << MXCSR_ROUND_SHIFT)) |
	    ((uint32_t)round << MXCSR_ROUND_SHIFT));
	return 0;
}

int
fegetenv(fenv_t *envp)
{
	struct x87env e;
	x87_get(&e);
	envp->__control = (unsigned short)e.control;
	envp->__status = (unsigned short)e.status;
	envp->__mxcsr = mxcsr_get();
	for (int i = 0; i < 8; i++)
		envp->__reserved[i] = 0;
	return 0;
}

int
fesetenv(const fenv_t *envp)
{
	struct x87env e;
	x87_get(&e);
	e.control = (e.control & 0xffff0000u) | envp->__control;
	e.status = (e.status & 0xffff0000u) | envp->__status;
	x87_set(&e);
	mxcsr_set(envp->__mxcsr);
	return 0;
}

/* The environment saved, the flags cleared, every exception masked. */
int
feholdexcept(fenv_t *envp)
{
	struct x87env e;
	fegetenv(envp);
	x87_get(&e);
	e.status &= ~(uint32_t)FE_ALL_EXCEPT;
	e.control |= FE_ALL_EXCEPT;
	x87_set(&e);
	mxcsr_set((mxcsr_get() & ~(uint32_t)FE_ALL_EXCEPT) |
	    ((uint32_t)FE_ALL_EXCEPT << MXCSR_MASK_SHIFT));
	return 0;
}

/* The environment installed, then the exceptions raised meanwhile raised
 * again in it. */
int
feupdateenv(const fenv_t *envp)
{
	int raised = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(envp);
	feraiseexcept(raised);
	return 0;
}

/* Not C99's nor macOS's: kept for Minkar programs that used msun's. */
int feenableexcept(int mask);
int fedisableexcept(int mask);

int
feenableexcept(int mask)
{
	struct x87env e;
	uint32_t m = mxcsr_get(), old = ~(m >> MXCSR_MASK_SHIFT) & FE_ALL_EXCEPT;
	mask &= FE_ALL_EXCEPT;
	x87_get(&e);
	e.control &= ~(uint32_t)mask;
	x87_set(&e);
	mxcsr_set(m & ~((uint32_t)mask << MXCSR_MASK_SHIFT));
	return (int)old;
}

int
fedisableexcept(int mask)
{
	struct x87env e;
	uint32_t m = mxcsr_get(), old = ~(m >> MXCSR_MASK_SHIFT) & FE_ALL_EXCEPT;
	mask &= FE_ALL_EXCEPT;
	x87_get(&e);
	e.control |= (uint32_t)mask;
	x87_set(&e);
	mxcsr_set(m | ((uint32_t)mask << MXCSR_MASK_SHIFT));
	return (int)old;
}
