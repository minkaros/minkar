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
 * <fenv.h> for Minkar, compatible with macOS's on x86-64: C99's floating
 * point environment (libsystem_m) over the x87 unit and SSE together, with
 * macOS's layout of fenv_t (16 bytes: the x87 control and status words,
 * then MXCSR) and its default environments, so that binaries built for the
 * reference system run unchanged. Written for Minkar.
 */

#ifndef _FENV_H_
#define	_FENV_H_

#include <sys/cdefs.h>

typedef struct {
	unsigned short	__control;	/* x87 control word */
	unsigned short	__status;	/* x87 status word */
	unsigned int	__mxcsr;	/* SSE control and status */
	char		__reserved[8];
} fenv_t;

typedef unsigned short fexcept_t;

/* Exceptions: the x87 status word's and MXCSR's flag bits. */
#define	FE_INEXACT		0x0020
#define	FE_UNDERFLOW		0x0010
#define	FE_OVERFLOW		0x0008
#define	FE_DIVBYZERO		0x0004
#define	FE_INVALID		0x0001
#define	FE_DENORMALOPERAND	0x0002	/* not C99's */
#define	FE_ALL_EXCEPT		0x003f

/* Rounding: the x87 control word's rounding control. */
#define	FE_TONEAREST		0x0000
#define	FE_DOWNWARD		0x0400
#define	FE_UPWARD		0x0800
#define	FE_TOWARDZERO		0x0c00

/* The default environment, and the same with SSE denormals flushed to
 * zero and treated as zero. */
extern const fenv_t _FE_DFL_ENV;
#define	FE_DFL_ENV		(&_FE_DFL_ENV)
extern const fenv_t _FE_DFL_DISABLE_SSE_DENORMS_ENV;
#define	FE_DFL_DISABLE_SSE_DENORMS_ENV	(&_FE_DFL_DISABLE_SSE_DENORMS_ENV)

__BEGIN_DECLS
int	feclearexcept(int);
int	fegetexceptflag(fexcept_t *, int);
int	feraiseexcept(int);
int	fesetexceptflag(const fexcept_t *, int);
int	fetestexcept(int);
int	fegetround(void);
int	fesetround(int);
int	fegetenv(fenv_t *);
int	feholdexcept(fenv_t *);
int	fesetenv(const fenv_t *);
int	feupdateenv(const fenv_t *);
__END_DECLS

#endif /* !_FENV_H_ */
