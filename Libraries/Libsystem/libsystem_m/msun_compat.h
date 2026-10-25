/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * What FreeBSD's msun expects from FreeBSD's <sys/cdefs.h> and libc that
 * Darwin lacks, force-included when building libsystem_m from msun.
 */

#ifndef _MINKAR_MSUN_COMPAT_H
#define _MINKAR_MSUN_COMPAT_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

/* FreeBSD's visibility macros: everything, as FreeBSD's msun build sees. */
#ifndef __ISO_C_VISIBLE
#define	__ISO_C_VISIBLE		2011
#endif
#ifndef __BSD_VISIBLE
#define	__BSD_VISIBLE		1
#endif
#ifndef __XSI_VISIBLE
#define	__XSI_VISIBLE		700
#endif
#ifndef __POSIX_VISIBLE
#define	__POSIX_VISIBLE		200809
#endif
#ifndef __always_inline
#define	__always_inline		__inline __attribute__((__always_inline__))
#endif

/* FreeBSD's __CONCAT expands its arguments first (0x1.8p ## LDBL_MANT_DIG). */
#undef	__CONCAT
#define	__CONCAT1(x, y)	x ## y
#define	__CONCAT(x, y)	__CONCAT1(x, y)
#ifndef __aligned
#define	__aligned(x)	__attribute__((__aligned__(x)))
#endif
#ifndef __predict_false
#define	__predict_true(e)	__builtin_expect(!!(e), 1)
#define	__predict_false(e)	__builtin_expect(!!(e), 0)
#endif
#include <limits.h>
#define	__INT_MAX	INT_MAX

/* FreeBSD's <machine/_types.h> on amd64: float_t and double_t. */
typedef float	__float_t;
typedef double	__double_t;

/* sym's other name: a Mach-O alias of the same address. */
#define	__msun_alias(sym, alias)					\
	__asm__(".globl _" #alias "\n\t.set _" #alias ", _" #sym)
#ifndef __weak_reference
#define	__weak_reference(sym, alias)	__msun_alias(sym, alias)
#endif
#ifndef __strong_reference
#define	__strong_reference(sym, alias)	__msun_alias(sym, alias)
#endif

#endif /* !_MINKAR_MSUN_COMPAT_H */
