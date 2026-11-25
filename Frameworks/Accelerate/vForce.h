/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * vForce: elementary functions over arrays, as Accelerate documents them
 * (results first, then the arguments, then a pointer to the count).
 */

#ifndef __VFORCE__
#define __VFORCE__

#ifdef __cplusplus
extern "C" {
#endif

void vvsqrtf(float *y, const float *x, const int *n);
void vvsqrt(double *y, const double *x, const int *n);
void vvexpf(float *y, const float *x, const int *n);
void vvexp(double *y, const double *x, const int *n);
void vvlogf(float *y, const float *x, const int *n);
void vvlog(double *y, const double *x, const int *n);
void vvlog10f(float *y, const float *x, const int *n);
void vvlog10(double *y, const double *x, const int *n);
void vvsinf(float *y, const float *x, const int *n);
void vvsin(double *y, const double *x, const int *n);
void vvcosf(float *y, const float *x, const int *n);
void vvcos(double *y, const double *x, const int *n);
void vvsincosf(float *sine, float *cosine, const float *x, const int *n);
void vvsincos(double *sine, double *cosine, const double *x, const int *n);
void vvatan2f(float *z, const float *y, const float *x, const int *n);
void vvatan2(double *z, const double *y, const double *x, const int *n);
void vvpowf(float *z, const float *y, const float *x, const int *n);
void vvpow(double *z, const double *y, const double *x, const int *n);
void vvfabsf(float *y, const float *x, const int *n);
void vvfabs(double *y, const double *x, const int *n);

#ifdef __cplusplus
}
#endif

#endif
