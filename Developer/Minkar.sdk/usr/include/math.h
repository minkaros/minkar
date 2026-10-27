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
 * <math.h> for Minkar, compatible with macOS's: the C99 functions in
 * their float, double and long double forms (libsystem_m, which is
 * FreeBSD's msun), type-generic classification and comparison macros on
 * the compiler's builtins, macOS's FP_* values and the usual M_*
 * constants. Written for Minkar.
 */

#ifndef _MATH_H_
#define	_MATH_H_

#include <sys/cdefs.h>

/* The types the compiler evaluates float and double expressions in. */
#if __FLT_EVAL_METHOD__ == 2
typedef long double	float_t;
typedef long double	double_t;
#elif __FLT_EVAL_METHOD__ == 1
typedef double		float_t;
typedef double		double_t;
#else
typedef float		float_t;
typedef double		double_t;
#endif

#define	HUGE_VAL	__builtin_huge_val()
#define	HUGE_VALF	__builtin_huge_valf()
#define	HUGE_VALL	__builtin_huge_vall()
#define	INFINITY	__builtin_inff()
#define	NAN		__builtin_nanf("")

/* fpclassify() results, with macOS's values. */
#define	FP_NAN		1
#define	FP_INFINITE	2
#define	FP_ZERO		3
#define	FP_NORMAL	4
#define	FP_SUBNORMAL	5
#define	FP_SUPERNORMAL	6	/* never returned on x86 */
#define	FP_QNAN		FP_NAN
#define	FP_SNAN		FP_NAN

/* What ilogb() returns for zero and NaN in libsystem_m. */
#define	FP_ILOGB0	(-2147483647)
#define	FP_ILOGBNAN	2147483647

#define	MATH_ERRNO	1
#define	MATH_ERREXCEPT	2
#define	math_errhandling	MATH_ERREXCEPT

#ifdef __FP_FAST_FMA
#define	FP_FAST_FMA	1
#endif
#ifdef __FP_FAST_FMAF
#define	FP_FAST_FMAF	1
#endif
#ifdef __FP_FAST_FMAL
#define	FP_FAST_FMAL	1
#endif

/* Classification of any floating type, as on macOS. */
#define	fpclassify(x)	__builtin_fpclassify(FP_NAN, FP_INFINITE, \
			    FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define	isfinite(x)	__builtin_isfinite(x)
#define	isinf(x)	__builtin_isinf(x)
#define	isnan(x)	__builtin_isnan(x)
#define	isnormal(x)	__builtin_isnormal(x)
#define	signbit(x)	__builtin_signbit(x)

/* Comparisons that raise no exception on NaN. */
#define	isgreater(x, y)		__builtin_isgreater((x), (y))
#define	isgreaterequal(x, y)	__builtin_isgreaterequal((x), (y))
#define	isless(x, y)		__builtin_isless((x), (y))
#define	islessequal(x, y)	__builtin_islessequal((x), (y))
#define	islessgreater(x, y)	__builtin_islessgreater((x), (y))
#define	isunordered(x, y)	__builtin_isunordered((x), (y))

#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE) || \
    defined(_XOPEN_SOURCE)
#define	M_E		2.71828182845904523536028747135266250	/* e */
#define	M_LOG2E		1.44269504088896340735992468100189214	/* log2(e) */
#define	M_LOG10E	0.434294481903251827651128918916605082	/* log10(e) */
#define	M_LN2		0.693147180559945309417232121458176568	/* ln(2) */
#define	M_LN10		2.30258509299404568401799145468436421	/* ln(10) */
#define	M_PI		3.14159265358979323846264338327950288	/* pi */
#define	M_PI_2		1.57079632679489661923132169163975144	/* pi/2 */
#define	M_PI_4		0.785398163397448309615660845819875721	/* pi/4 */
#define	M_1_PI		0.318309886183790671537767526745028724	/* 1/pi */
#define	M_2_PI		0.636619772367581343075535053490057448	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257389615890312154517	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880168872420969808	/* sqrt(2) */
#define	M_SQRT1_2	0.707106781186547524400844362104849039	/* 1/sqrt(2) */
#define	MAXFLOAT	0x1.fffffep+127f
#define	HUGE		MAXFLOAT
#endif

__BEGIN_DECLS

extern float acosf(float);
extern double acos(double);
extern long double acosl(long double);

extern float asinf(float);
extern double asin(double);
extern long double asinl(long double);

extern float atanf(float);
extern double atan(double);
extern long double atanl(long double);

extern float atan2f(float, float);
extern double atan2(double, double);
extern long double atan2l(long double, long double);

extern float cosf(float);
extern double cos(double);
extern long double cosl(long double);

extern float sinf(float);
extern double sin(double);
extern long double sinl(long double);

extern float tanf(float);
extern double tan(double);
extern long double tanl(long double);

extern float acoshf(float);
extern double acosh(double);
extern long double acoshl(long double);

extern float asinhf(float);
extern double asinh(double);
extern long double asinhl(long double);

extern float atanhf(float);
extern double atanh(double);
extern long double atanhl(long double);

extern float coshf(float);
extern double cosh(double);
extern long double coshl(long double);

extern float sinhf(float);
extern double sinh(double);
extern long double sinhl(long double);

extern float tanhf(float);
extern double tanh(double);
extern long double tanhl(long double);

extern float expf(float);
extern double exp(double);
extern long double expl(long double);

extern float exp2f(float);
extern double exp2(double);
extern long double exp2l(long double);

extern float expm1f(float);
extern double expm1(double);
extern long double expm1l(long double);

extern float logf(float);
extern double log(double);
extern long double logl(long double);

extern float log10f(float);
extern double log10(double);
extern long double log10l(long double);

extern float log2f(float);
extern double log2(double);
extern long double log2l(long double);

extern float log1pf(float);
extern double log1p(double);
extern long double log1pl(long double);

extern float logbf(float);
extern double logb(double);
extern long double logbl(long double);

extern float modff(float, float *);
extern double modf(double, double *);
extern long double modfl(long double, long double *);

extern float ldexpf(float, int);
extern double ldexp(double, int);
extern long double ldexpl(long double, int);

extern float frexpf(float, int *);
extern double frexp(double, int *);
extern long double frexpl(long double, int *);

extern int ilogbf(float);
extern int ilogb(double);
extern int ilogbl(long double);

extern float scalbnf(float, int);
extern double scalbn(double, int);
extern long double scalbnl(long double, int);

extern float scalblnf(float, long);
extern double scalbln(double, long);
extern long double scalblnl(long double, long);

extern float fabsf(float);
extern double fabs(double);
extern long double fabsl(long double);

extern float cbrtf(float);
extern double cbrt(double);
extern long double cbrtl(long double);

extern float hypotf(float, float);
extern double hypot(double, double);
extern long double hypotl(long double, long double);

extern float powf(float, float);
extern double pow(double, double);
extern long double powl(long double, long double);

extern float sqrtf(float);
extern double sqrt(double);
extern long double sqrtl(long double);

extern float erff(float);
extern double erf(double);
extern long double erfl(long double);

extern float erfcf(float);
extern double erfc(double);
extern long double erfcl(long double);

extern float lgammaf(float);
extern double lgamma(double);
extern long double lgammal(long double);

extern float tgammaf(float);
extern double tgamma(double);
extern long double tgammal(long double);

extern float ceilf(float);
extern double ceil(double);
extern long double ceill(long double);

extern float floorf(float);
extern double floor(double);
extern long double floorl(long double);

extern float nearbyintf(float);
extern double nearbyint(double);
extern long double nearbyintl(long double);

extern float rintf(float);
extern double rint(double);
extern long double rintl(long double);

extern long lrintf(float);
extern long lrint(double);
extern long lrintl(long double);

extern long long llrintf(float);
extern long long llrint(double);
extern long long llrintl(long double);

extern float roundf(float);
extern double round(double);
extern long double roundl(long double);

extern long lroundf(float);
extern long lround(double);
extern long lroundl(long double);

extern long long llroundf(float);
extern long long llround(double);
extern long long llroundl(long double);

extern float truncf(float);
extern double trunc(double);
extern long double truncl(long double);

extern float fmodf(float, float);
extern double fmod(double, double);
extern long double fmodl(long double, long double);

extern float remainderf(float, float);
extern double remainder(double, double);
extern long double remainderl(long double, long double);

extern float remquof(float, float, int *);
extern double remquo(double, double, int *);
extern long double remquol(long double, long double, int *);

extern float copysignf(float, float);
extern double copysign(double, double);
extern long double copysignl(long double, long double);

extern float nanf(const char *);
extern double nan(const char *);
extern long double nanl(const char *);

extern float nextafterf(float, float);
extern double nextafter(double, double);
extern long double nextafterl(long double, long double);

extern float nexttowardf(float, long double);
extern double nexttoward(double, long double);
extern long double nexttowardl(long double, long double);

extern float fdimf(float, float);
extern double fdim(double, double);
extern long double fdiml(long double, long double);

extern float fmaxf(float, float);
extern double fmax(double, double);
extern long double fmaxl(long double, long double);

extern float fminf(float, float);
extern double fmin(double, double);
extern long double fminl(long double, long double);

extern float fmaf(float, float, float);
extern double fma(double, double, double);
extern long double fmal(long double, long double, long double);

/* X/Open: Bessel functions, scalb, and lgamma's sign. */
extern int signgam;
extern double j0(double);
extern double j1(double);
extern double jn(int, double);
extern double y0(double);
extern double y1(double);
extern double yn(int, double);
extern double scalb(double, double);
extern double lgamma_r(double, int *);
extern float lgammaf_r(float, int *);

/*
 * Sine and cosine at once. The compiler calls these when a program
 * computes both of the same value; both values come back in registers.
 */
struct __float2 { float __sinval; float __cosval; };
struct __double2 { double __sinval; double __cosval; };
extern struct __float2 __sincosf_stret(float);
extern struct __double2 __sincos_stret(double);

__END_DECLS

#endif /* !_MATH_H_ */
