/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * libsystem_m's names that FreeBSD's msun does not have: the ones
 * ravynOS's <math.h> declares as functions (fpclassify, isnan, isgreater,
 * ...), ldexp, the __isinf FreeBSD's libc gives msun, and Darwin's
 * __exp10 and __sincos_stret.
 */

#include <float.h>
#include <stdbool.h>

double	pow(double, double);
float	powf(float, float);
void	sincos(double, double *, double *);
void	sincosf(float, float *, float *);

#define	FP_NAN_		1
#define	FP_INFINITE_	2
#define	FP_ZERO_	3
#define	FP_NORMAL_	4
#define	FP_SUBNORMAL_	5

int	fpclassify(double x);
bool	isfinite(double x);
bool	isinf(double x);
bool	isnormal(double x);
bool	signbit(double x);
bool	isgreater(double x, double y);
bool	isgreaterequal(double x, double y);
bool	isless(double x, double y);
bool	islessequal(double x, double y);
bool	islessgreater(double x, double y);
bool	isunordered(double x, double y);
double	__exp10(double x);
float	__exp10f(float x);

int
fpclassify(double x)
{
	return (__builtin_fpclassify(FP_NAN_, FP_INFINITE_, FP_NORMAL_,
	    FP_SUBNORMAL_, FP_ZERO_, x));
}

bool isfinite(double x) { return (__builtin_isfinite(x)); }
bool isinf(double x) { return (__builtin_isinf(x)); }
bool isnormal(double x) { return (__builtin_isnormal(x)); }
bool signbit(double x) { return (__builtin_signbit(x) != 0); }
bool isgreater(double x, double y) { return (__builtin_isgreater(x, y)); }
bool isgreaterequal(double x, double y) { return (__builtin_isgreaterequal(x, y)); }
bool isless(double x, double y) { return (__builtin_isless(x, y)); }
bool islessequal(double x, double y) { return (__builtin_islessequal(x, y)); }
bool islessgreater(double x, double y) { return (__builtin_islessgreater(x, y)); }
bool isunordered(double x, double y) { return (__builtin_isunordered(x, y)); }

double __exp10(double x) { return (pow(10.0, x)); }
float __exp10f(float x) { return (powf(10.0f, x)); }

/* ldexp: scalbn (the assembly aliases ldexpf and ldexpl itself). */
double	scalbn(double, int);
double	ldexp(double x, int n);
double ldexp(double x, int n) { return (scalbn(x, n)); }

/* FreeBSD's libc has these; msun uses them. */
int	__isinf(double x);
int	__isinff(float x);
int	__isinfl(long double x);
int __isinf(double x) { return (__builtin_isinf(x)); }
int __isinff(float x) { return (__builtin_isinf(x)); }
int __isinfl(long double x) { return (__builtin_isinf(x)); }

struct __double2 { double __sinval; double __cosval; };
struct __float2 { float __sinval; float __cosval; };
struct __double2 __sincos_stret(double x);
struct __float2 __sincosf_stret(float x);

struct __double2
__sincos_stret(double x)
{
	struct __double2 r;

	sincos(x, &r.__sinval, &r.__cosval);
	return (r);
}

struct __float2
__sincosf_stret(float x)
{
	struct __float2 r;

	sincosf(x, &r.__sinval, &r.__cosval);
	return (r);
}
