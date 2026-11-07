/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The special results of the Arm pow (libm.h): each is computed, not a
 * constant, so the operation raises its floating-point exception.
 */

#include "libm.h"

static double xflow(uint32_t sign, double y)
{
	return eval_as_double(fp_barrier(sign ? -y : y) * y);
}

double __math_oflow(uint32_t sign)
{
	return xflow(sign, 0x1p769);
}

double __math_uflow(uint32_t sign)
{
	return xflow(sign, 0x1p-767);
}

double __math_invalid(double x)
{
	return (x - x) / (x - x);
}
