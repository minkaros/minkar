/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * vDSP (vDSP.h): written from the documented definitions of each
 * function. The Fourier transforms are an iterative radix-2 transform,
 * computed in double precision for both forms; a setup holds the
 * twiddle factors for its largest size, smaller sizes step through them.
 */

#include "vDSP.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- The Fourier transform ---- */

struct OpaqueFFTSetup {
    vDSP_Length log2n;
    double *cosine, *sine; /* e^{-2 pi i k / N}, k < N/2, N = 2^log2n */
};

static struct OpaqueFFTSetup *createSetup(vDSP_Length log2n, FFTRadix radix) {
    if (radix != kFFTRadix2 || log2n > 24)
        return NULL;
    struct OpaqueFFTSetup *s = calloc(1, sizeof(*s));
    size_t half = ((size_t)1 << log2n) / 2;
    if (s == NULL)
        return NULL;
    s->log2n = log2n;
    s->cosine = malloc((half + 1) * sizeof(double));
    s->sine = malloc((half + 1) * sizeof(double));
    if (s->cosine == NULL || s->sine == NULL) {
        free(s->cosine);
        free(s->sine);
        free(s);
        return NULL;
    }
    for (size_t k = 0; k <= half; k++) {
        double angle = -2.0 * M_PI * (double)k / (double)((size_t)1 << log2n);
        s->cosine[k] = cos(angle);
        s->sine[k] = sin(angle);
    }
    return s;
}

static void destroySetup(struct OpaqueFFTSetup *s) {
    if (s != NULL) {
        free(s->cosine);
        free(s->sine);
        free(s);
    }
}

FFTSetup vDSP_create_fftsetup(vDSP_Length log2n, FFTRadix radix) { return (FFTSetup)createSetup(log2n, radix); }
void vDSP_destroy_fftsetup(FFTSetup setup) { destroySetup((struct OpaqueFFTSetup *)setup); }
FFTSetupD vDSP_create_fftsetupD(vDSP_Length log2n, FFTRadix radix) { return (FFTSetupD)createSetup(log2n, radix); }
void vDSP_destroy_fftsetupD(FFTSetupD setup) { destroySetup((struct OpaqueFFTSetup *)setup); }

/* In place on n = 2^log2n complex values; forward e^{-i}, inverse e^{+i},
   unscaled. */
static void transform(const struct OpaqueFFTSetup *s, double *re, double *im, vDSP_Length log2n, int direction) {
    size_t n = (size_t)1 << log2n;
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j |= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    double sign = direction == kFFTDirection_Inverse ? -1.0 : 1.0;
    for (size_t length = 2; length <= n; length <<= 1) {
        size_t step = ((size_t)1 << s->log2n) / length, half = length / 2;
        for (size_t start = 0; start < n; start += length)
            for (size_t k = 0; k < half; k++) {
                double wr = s->cosine[k * step], wi = sign * s->sine[k * step];
                size_t a = start + k, b = a + half;
                double xr = re[b] * wr - im[b] * wi, xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
            }
    }
}

static int usable(const void *setup, vDSP_Length log2n) {
    return setup != NULL && log2n <= ((const struct OpaqueFFTSetup *)setup)->log2n;
}

#define COMPLEX_FFT(T, SPLIT, NAME)                                                                          \
    static void NAME(const void *setup, const SPLIT *a, vDSP_Stride ia, const SPLIT *c, vDSP_Stride ic,       \
                     vDSP_Length log2n, FFTDirection direction) {                                             \
        size_t n = (size_t)1 << log2n;                                                                        \
        double *re, *im;                                                                                      \
        if (!usable(setup, log2n) || (re = malloc(2 * n * sizeof(double))) == NULL)                           \
            return;                                                                                           \
        im = re + n;                                                                                          \
        for (size_t k = 0; k < n; k++) {                                                                      \
            re[k] = a->realp[k * ia];                                                                         \
            im[k] = a->imagp[k * ia];                                                                         \
        }                                                                                                     \
        transform(setup, re, im, log2n, direction);                                                           \
        for (size_t k = 0; k < n; k++) {                                                                      \
            c->realp[k * ic] = (T)re[k];                                                                      \
            c->imagp[k * ic] = (T)im[k];                                                                      \
        }                                                                                                     \
        free(re);                                                                                             \
    }
COMPLEX_FFT(float, DSPSplitComplex, complexFFT)
COMPLEX_FFT(double, DSPDoubleSplitComplex, complexFFTD)

void vDSP_fft_zip(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n, FFTDirection direction) {
    complexFFT(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zipt(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, const DSPSplitComplex *temp,
                   vDSP_Length log2n, FFTDirection direction) {
    complexFFT(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zop(FFTSetup setup, const DSPSplitComplex *a, vDSP_Stride ia, const DSPSplitComplex *c,
                  vDSP_Stride ic, vDSP_Length log2n, FFTDirection direction) {
    complexFFT(setup, a, ia, c, ic, log2n, direction);
}
void vDSP_fft_zipD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                   FFTDirection direction) {
    complexFFTD(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_ziptD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride,
                    const DSPDoubleSplitComplex *temp, vDSP_Length log2n, FFTDirection direction) {
    complexFFTD(setup, c, stride, c, stride, log2n, direction);
}

/* The real transform, packed: n real values as n/2 complex ones (even
   values in realp, odd in imagp). Forward gives 2X[k] for k < n/2, with
   2X[n/2] (real) in imagp[0]; inverse takes that form and gives the
   unscaled inverse transform of the whole spectrum. */
#define REAL_FFT(T, SPLIT, NAME)                                                                             \
    static void NAME(const void *setup, const SPLIT *a, vDSP_Stride ia, const SPLIT *c, vDSP_Stride ic,       \
                     vDSP_Length log2n, FFTDirection direction) {                                             \
        size_t n = (size_t)1 << log2n, half = n / 2;                                                          \
        double *re, *im;                                                                                      \
        if (log2n < 1 || !usable(setup, log2n) || (re = malloc(2 * n * sizeof(double))) == NULL)              \
            return;                                                                                           \
        im = re + n;                                                                                          \
        if (direction != kFFTDirection_Inverse) {                                                             \
            for (size_t k = 0; k < half; k++) {                                                               \
                re[2 * k] = a->realp[k * ia];                                                                 \
                re[2 * k + 1] = a->imagp[k * ia];                                                             \
                im[2 * k] = im[2 * k + 1] = 0;                                                                \
            }                                                                                                 \
            transform(setup, re, im, log2n, kFFTDirection_Forward);                                           \
            c->realp[0] = (T)(2 * re[0]);                                                                     \
            c->imagp[0] = (T)(2 * re[half]);                                                                  \
            for (size_t k = 1; k < half; k++) {                                                               \
                c->realp[k * ic] = (T)(2 * re[k]);                                                            \
                c->imagp[k * ic] = (T)(2 * im[k]);                                                            \
            }                                                                                                 \
        } else {                                                                                              \
            re[0] = a->realp[0];                                                                              \
            im[0] = 0;                                                                                        \
            re[half] = a->imagp[0];                                                                           \
            im[half] = 0;                                                                                     \
            for (size_t k = 1; k < half; k++) {                                                               \
                re[k] = re[n - k] = a->realp[k * ia];                                                         \
                im[k] = a->imagp[k * ia];                                                                     \
                im[n - k] = -im[k];                                                                           \
            }                                                                                                 \
            transform(setup, re, im, log2n, kFFTDirection_Inverse);                                           \
            for (size_t k = 0; k < half; k++) {                                                               \
                c->realp[k * ic] = (T)re[2 * k];                                                              \
                c->imagp[k * ic] = (T)re[2 * k + 1];                                                          \
            }                                                                                                 \
        }                                                                                                     \
        free(re);                                                                                             \
    }
REAL_FFT(float, DSPSplitComplex, realFFT)
REAL_FFT(double, DSPDoubleSplitComplex, realFFTD)

void vDSP_fft_zrip(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                   FFTDirection direction) {
    realFFT(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zript(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, const DSPSplitComplex *temp,
                    vDSP_Length log2n, FFTDirection direction) {
    realFFT(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zripD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                    FFTDirection direction) {
    realFFTD(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zriptD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride,
                     const DSPDoubleSplitComplex *temp, vDSP_Length log2n, FFTDirection direction) {
    realFFTD(setup, c, stride, c, stride, log2n, direction);
}
void vDSP_fft_zrop(FFTSetup setup, const DSPSplitComplex *a, vDSP_Stride ia, const DSPSplitComplex *c,
                   vDSP_Stride ic, vDSP_Length log2n, FFTDirection direction) {
    realFFT(setup, a, ia, c, ic, log2n, direction);
}

/* ---- Complex vectors ---- */

/* The interleaved stride counts the interleaved array's scalars (2 for
   consecutive values). */
void vDSP_ctoz(const DSPComplex *c, vDSP_Stride ic, const DSPSplitComplex *z, vDSP_Stride iz, vDSP_Length n) {
    const float *p = (const float *)c;
    for (vDSP_Length k = 0; k < n; k++) {
        z->realp[k * iz] = p[k * ic];
        z->imagp[k * iz] = p[k * ic + 1];
    }
}
void vDSP_ztoc(const DSPSplitComplex *z, vDSP_Stride iz, DSPComplex *c, vDSP_Stride ic, vDSP_Length n) {
    float *p = (float *)c;
    for (vDSP_Length k = 0; k < n; k++) {
        p[k * ic] = z->realp[k * iz];
        p[k * ic + 1] = z->imagp[k * iz];
    }
}
void vDSP_ctozD(const DSPDoubleComplex *c, vDSP_Stride ic, const DSPDoubleSplitComplex *z, vDSP_Stride iz, vDSP_Length n) {
    const double *p = (const double *)c;
    for (vDSP_Length k = 0; k < n; k++) {
        z->realp[k * iz] = p[k * ic];
        z->imagp[k * iz] = p[k * ic + 1];
    }
}
void vDSP_ztocD(const DSPDoubleSplitComplex *z, vDSP_Stride iz, DSPDoubleComplex *c, vDSP_Stride ic, vDSP_Length n) {
    double *p = (double *)c;
    for (vDSP_Length k = 0; k < n; k++) {
        p[k * ic] = z->realp[k * iz];
        p[k * ic + 1] = z->imagp[k * iz];
    }
}

void vDSP_zvmags(const DSPSplitComplex *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++) {
        float r = a->realp[k * ia], i = a->imagp[k * ia];
        c[k * ic] = r * r + i * i;
    }
}
void vDSP_zvmagsD(const DSPDoubleSplitComplex *a, vDSP_Stride ia, double *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++) {
        double r = a->realp[k * ia], i = a->imagp[k * ia];
        c[k * ic] = r * r + i * i;
    }
}
void vDSP_zvabs(const DSPSplitComplex *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = hypotf(a->realp[k * ia], a->imagp[k * ia]);
}

#define ZVMUL(T, SPLIT, NAME)                                                                                \
    void NAME(const SPLIT *a, vDSP_Stride ia, const SPLIT *b, vDSP_Stride ib, const SPLIT *c, vDSP_Stride ic, \
              vDSP_Length n, int conjugate) {                                                                 \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            T ar = a->realp[k * ia], ai = a->imagp[k * ia], br = b->realp[k * ib], bi = b->imagp[k * ib];     \
            if (conjugate == -1)                                                                              \
                ai = -ai;                                                                                     \
            c->realp[k * ic] = ar * br - ai * bi;                                                             \
            c->imagp[k * ic] = ar * bi + ai * br;                                                             \
        }                                                                                                     \
    }
ZVMUL(float, DSPSplitComplex, vDSP_zvmul)
ZVMUL(double, DSPDoubleSplitComplex, vDSP_zvmulD)

/* ---- Real vectors ---- */

#define BINARY(NAME, T, EXPR)                                                                                \
    void NAME(const T *a, vDSP_Stride ia, const T *b, vDSP_Stride ib, T *c, vDSP_Stride ic, vDSP_Length n) {  \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            T x = a[k * ia], y = b[k * ib];                                                                   \
            c[k * ic] = (EXPR);                                                                               \
        }                                                                                                     \
    }
BINARY(vDSP_vadd, float, x + y)
BINARY(vDSP_vaddD, double, x + y)
BINARY(vDSP_vmul, float, x * y)
BINARY(vDSP_vmulD, double, x * y)
/* vsub(B, A) = A - B; vdiv(B, A) = A / B: the first argument is B. */
BINARY(vDSP_vsub, float, y - x)
BINARY(vDSP_vsubD, double, y - x)
BINARY(vDSP_vdiv, float, y / x)

#define SCALAR(NAME, T, EXPR)                                                                                \
    void NAME(const T *a, vDSP_Stride ia, const T *b, T *c, vDSP_Stride ic, vDSP_Length n) {                  \
        T s = *b;                                                                                             \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            T x = a[k * ia];                                                                                  \
            c[k * ic] = (EXPR);                                                                               \
        }                                                                                                     \
    }
SCALAR(vDSP_vsadd, float, x + s)
SCALAR(vDSP_vsaddD, double, x + s)
SCALAR(vDSP_vsmul, float, x * s)
SCALAR(vDSP_vsmulD, double, x * s)
SCALAR(vDSP_vsdiv, float, x / s)
SCALAR(vDSP_vsdivD, double, x / s)

void vDSP_vsma(const float *a, vDSP_Stride ia, const float *b, const float *c, vDSP_Stride ic, float *d,
               vDSP_Stride id, vDSP_Length n) {
    float s = *b;
    for (vDSP_Length k = 0; k < n; k++)
        d[k * id] = a[k * ia] * s + c[k * ic];
}
void vDSP_vsmaD(const double *a, vDSP_Stride ia, const double *b, const double *c, vDSP_Stride ic, double *d,
                vDSP_Stride id, vDSP_Length n) {
    double s = *b;
    for (vDSP_Length k = 0; k < n; k++)
        d[k * id] = a[k * ia] * s + c[k * ic];
}
/* d = a * b + c, b and c scalars. */
void vDSP_vsmsa(const float *a, vDSP_Stride ia, const float *b, const float *c, float *d, vDSP_Stride id,
                vDSP_Length n) {
    float s = *b, t = *c;
    for (vDSP_Length k = 0; k < n; k++)
        d[k * id] = a[k * ia] * s + t;
}
void vDSP_vsmsaD(const double *a, vDSP_Stride ia, const double *b, const double *c, double *d, vDSP_Stride id,
                 vDSP_Length n) {
    double s = *b, t = *c;
    for (vDSP_Length k = 0; k < n; k++)
        d[k * id] = a[k * ia] * s + t;
}
void vDSP_vma(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, const float *c, vDSP_Stride ic,
              float *d, vDSP_Stride id, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        d[k * id] = a[k * ia] * b[k * ib] + c[k * ic];
}

void vDSP_vclip(const float *a, vDSP_Stride ia, const float *low, const float *high, float *c, vDSP_Stride ic,
                vDSP_Length n) {
    float lo = *low, hi = *high;
    for (vDSP_Length k = 0; k < n; k++) {
        float x = a[k * ia];
        c[k * ic] = x < lo ? lo : x > hi ? hi : x;
    }
}
void vDSP_vclipD(const double *a, vDSP_Stride ia, const double *low, const double *high, double *c, vDSP_Stride ic,
                 vDSP_Length n) {
    double lo = *low, hi = *high;
    for (vDSP_Length k = 0; k < n; k++) {
        double x = a[k * ia];
        c[k * ic] = x < lo ? lo : x > hi ? hi : x;
    }
}

void vDSP_vclr(float *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = 0;
}
void vDSP_vclrD(double *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = 0;
}
void vDSP_vfill(const float *a, float *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = *a;
}
void vDSP_vfillD(const double *a, double *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = *a;
}

#define UNARY(NAME, TIN, TOUT, EXPR)                                                                         \
    void NAME(const TIN *a, vDSP_Stride ia, TOUT *c, vDSP_Stride ic, vDSP_Length n) {                         \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            TIN x = a[k * ia];                                                                                \
            c[k * ic] = (TOUT)(EXPR);                                                                         \
        }                                                                                                     \
    }
UNARY(vDSP_vabs, float, float, fabsf(x))
UNARY(vDSP_vneg, float, float, -x)
UNARY(vDSP_vsq, float, float, x * x)
UNARY(vDSP_vdpsp, double, float, x)
UNARY(vDSP_vspdp, float, double, x)

void vDSP_vramp(const float *a, const float *b, float *c, vDSP_Stride ic, vDSP_Length n) {
    for (vDSP_Length k = 0; k < n; k++)
        c[k * ic] = *a + (float)k * *b;
}

/* ---- Reductions ---- */

void vDSP_dotpr(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, float *c, vDSP_Length n) {
    double sum = 0;
    for (vDSP_Length k = 0; k < n; k++)
        sum += (double)a[k * ia] * b[k * ib];
    *c = (float)sum;
}
void vDSP_dotprD(const double *a, vDSP_Stride ia, const double *b, vDSP_Stride ib, double *c, vDSP_Length n) {
    double sum = 0;
    for (vDSP_Length k = 0; k < n; k++)
        sum += a[k * ia] * b[k * ib];
    *c = sum;
}

#define REDUCE(NAME, T, INIT, STEP, RESULT)                                                                  \
    void NAME(const T *a, vDSP_Stride ia, T *c, vDSP_Length n) {                                              \
        double acc = (INIT);                                                                                  \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            double x = a[k * ia];                                                                             \
            STEP;                                                                                             \
        }                                                                                                     \
        *c = (T)(RESULT);                                                                                     \
    }
REDUCE(vDSP_sve, float, 0, acc += x, acc)
REDUCE(vDSP_svesq, float, 0, acc += x * x, acc)
REDUCE(vDSP_svesqD, double, 0, acc += x * x, acc)
REDUCE(vDSP_meanv, float, 0, acc += x, n > 0 ? acc / (double)n : 0)
REDUCE(vDSP_measqv, float, 0, acc += x * x, n > 0 ? acc / (double)n : 0)
REDUCE(vDSP_rmsqv, float, 0, acc += x * x, n > 0 ? sqrt(acc / (double)n) : 0)
REDUCE(vDSP_maxv, float, -INFINITY, if (x > acc) acc = x, acc)
REDUCE(vDSP_minv, float, INFINITY, if (x < acc) acc = x, acc)
REDUCE(vDSP_maxmgv, float, 0, if (fabs(x) > acc) acc = fabs(x), acc)
REDUCE(vDSP_maxmgvD, double, 0, if (fabs(x) > acc) acc = fabs(x), acc)

/* ---- Filters ---- */

#define CONV(NAME, T)                                                                                        \
    void NAME(const T *a, vDSP_Stride ia, const T *f, vDSP_Stride iff, T *c, vDSP_Stride ic, vDSP_Length n,   \
              vDSP_Length p) {                                                                                \
        for (vDSP_Length k = 0; k < n; k++) {                                                                 \
            double sum = 0;                                                                                   \
            for (vDSP_Length j = 0; j < p; j++)                                                               \
                sum += (double)a[(vDSP_Stride)(k + j) * ia] * f[(vDSP_Stride)j * iff];                         \
            c[k * ic] = (T)sum;                                                                               \
        }                                                                                                     \
    }
CONV(vDSP_conv, float)
CONV(vDSP_convD, double)

/* c[k] = a[k]b0 + a[k-1]b1 + a[k-2]b2 - c[k-1]b3 - c[k-2]b4 for k from
   2 to n+1: a and c hold n+2 values, c's first two the state. */
#define DEQ22(NAME, T)                                                                                       \
    void NAME(const T *a, vDSP_Stride ia, const T *b, T *c, vDSP_Stride ic, vDSP_Length n) {                  \
        for (vDSP_Length k = 2; k < n + 2; k++)                                                               \
            c[k * ic] = a[k * ia] * b[0] + a[(k - 1) * ia] * b[1] + a[(k - 2) * ia] * b[2] -                  \
                        c[(k - 1) * ic] * b[3] - c[(k - 2) * ic] * b[4];                                      \
    }
DEQ22(vDSP_deq22, float)
DEQ22(vDSP_deq22D, double)
