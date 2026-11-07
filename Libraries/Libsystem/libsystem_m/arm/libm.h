/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The helpers the Arm pow (pow.c, from musl 1.2.5, MIT) uses from musl's
 * internal libm.h, written here for libsystem_m.
 */

#ifndef _MINKAR_ARM_LIBM_H
#define _MINKAR_ARM_LIBM_H

#include <stdint.h>
#include <math.h>
#include "features.h"

#define WANT_ROUNDING 1
#define TOINT_INTRINSICS 0
#define issignaling_inline(x) 0

#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)

#define asuint64(f) ((union { double _f; uint64_t _i; }){ f })._i
#define asdouble(i) ((union { uint64_t _i; double _f; }){ i })._f

/* x86-64 has no excess precision: an assignment rounds to double. */
static inline double eval_as_double(double x)
{
	double y = x;
	return y;
}

/* Its input, kept from constant folding and code motion. */
static inline double fp_barrier(double x)
{
	volatile double y = x;
	return y;
}

/* Computes x for its floating-point exceptions. */
static inline void fp_force_eval(double x)
{
	volatile double y;
	y = x;
}

/* Results that raise overflow, underflow or invalid (math_err.c). */
hidden double __math_oflow(uint32_t sign);
hidden double __math_uflow(uint32_t sign);
hidden double __math_invalid(double x);

#endif
