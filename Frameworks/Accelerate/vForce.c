/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * vForce (vForce.h): each element through libm.
 */

#include "vForce.h"
#include <math.h>

#define ONE(NAME, T, F)                                     \
    void NAME(T *y, const T *x, const int *n) {             \
        for (int i = 0; i < *n; i++)                        \
            y[i] = F(x[i]);                                 \
    }
ONE(vvsqrtf, float, sqrtf)
ONE(vvsqrt, double, sqrt)
ONE(vvexpf, float, expf)
ONE(vvexp, double, exp)
ONE(vvlogf, float, logf)
ONE(vvlog, double, log)
ONE(vvlog10f, float, log10f)
ONE(vvlog10, double, log10)
ONE(vvsinf, float, sinf)
ONE(vvsin, double, sin)
ONE(vvcosf, float, cosf)
ONE(vvcos, double, cos)
ONE(vvfabsf, float, fabsf)
ONE(vvfabs, double, fabs)

void vvsincosf(float *s, float *c, const float *x, const int *n) {
    for (int i = 0; i < *n; i++) {
        float v = x[i];
        s[i] = sinf(v);
        c[i] = cosf(v);
    }
}
void vvsincos(double *s, double *c, const double *x, const int *n) {
    for (int i = 0; i < *n; i++) {
        double v = x[i];
        s[i] = sin(v);
        c[i] = cos(v);
    }
}

/* atan2(y, x) and pow(x, y): the documented order, y before x. */
#define TWO(NAME, T, EXPR)                                  \
    void NAME(T *z, const T *y, const T *x, const int *n) { \
        for (int i = 0; i < *n; i++)                        \
            z[i] = EXPR;                                    \
    }
TWO(vvatan2f, float, atan2f(y[i], x[i]))
TWO(vvatan2, double, atan2(y[i], x[i]))
TWO(vvpowf, float, powf(x[i], y[i]))
TWO(vvpow, double, pow(x[i], y[i]))
