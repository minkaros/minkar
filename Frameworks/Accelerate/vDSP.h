/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * vDSP: vector arithmetic and the fast Fourier transform, as Accelerate
 * documents them, in single and double precision. Strides are in
 * elements. The functions apps use (IINA, VLC, VSCodium) and those next
 * to them.
 */

#ifndef __VDSP__
#define __VDSP__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long vDSP_Stride;
typedef unsigned long vDSP_Length;

typedef struct DSPComplex { float real; float imag; } DSPComplex;
typedef struct DSPDoubleComplex { double real; double imag; } DSPDoubleComplex;
typedef struct DSPSplitComplex { float *realp; float *imagp; } DSPSplitComplex;
typedef struct DSPDoubleSplitComplex { double *realp; double *imagp; } DSPDoubleSplitComplex;

typedef struct OpaqueFFTSetup *FFTSetup;
typedef struct OpaqueFFTSetupD *FFTSetupD;

typedef int FFTDirection;
typedef int FFTRadix;
enum { kFFTDirection_Forward = 1, kFFTDirection_Inverse = -1 };
enum { FFT_FORWARD = kFFTDirection_Forward, FFT_INVERSE = kFFTDirection_Inverse };
enum { kFFTRadix2 = 0, kFFTRadix3 = 1, kFFTRadix5 = 2 };
enum { FFT_RADIX2 = kFFTRadix2, FFT_RADIX3 = kFFTRadix3, FFT_RADIX5 = kFFTRadix5 };

/* The Fourier transform: a setup for sizes up to 2^log2n (radix 2). */
FFTSetup vDSP_create_fftsetup(vDSP_Length log2n, FFTRadix radix);
void vDSP_destroy_fftsetup(FFTSetup setup);
FFTSetupD vDSP_create_fftsetupD(vDSP_Length log2n, FFTRadix radix);
void vDSP_destroy_fftsetupD(FFTSetupD setup);

/* Complex, in place (zip) or out of place (zop); the t forms take a
   temporary buffer (not needed here). Unscaled in both directions. */
void vDSP_fft_zip(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n, FFTDirection direction);
void vDSP_fft_zipt(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, const DSPSplitComplex *temp,
                   vDSP_Length log2n, FFTDirection direction);
void vDSP_fft_zop(FFTSetup setup, const DSPSplitComplex *a, vDSP_Stride ia, const DSPSplitComplex *c,
                  vDSP_Stride ic, vDSP_Length log2n, FFTDirection direction);
void vDSP_fft_zipD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                   FFTDirection direction);
void vDSP_fft_ziptD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride,
                    const DSPDoubleSplitComplex *temp, vDSP_Length log2n, FFTDirection direction);

/* Real, in place, packed: n = 2^log2n real values as n/2 complex ones
   (vDSP_ctoz); forward, realp[0] is the DC value and imagp[0] the
   Nyquist one, each value twice the transform's; inverse, the values
   times n (forward then inverse: times 2n). */
void vDSP_fft_zrip(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                   FFTDirection direction);
void vDSP_fft_zript(FFTSetup setup, const DSPSplitComplex *c, vDSP_Stride stride, const DSPSplitComplex *temp,
                    vDSP_Length log2n, FFTDirection direction);
void vDSP_fft_zripD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride, vDSP_Length log2n,
                    FFTDirection direction);
void vDSP_fft_zriptD(FFTSetupD setup, const DSPDoubleSplitComplex *c, vDSP_Stride stride,
                     const DSPDoubleSplitComplex *temp, vDSP_Length log2n, FFTDirection direction);
void vDSP_fft_zrop(FFTSetup setup, const DSPSplitComplex *a, vDSP_Stride ia, const DSPSplitComplex *c,
                   vDSP_Stride ic, vDSP_Length log2n, FFTDirection direction);

/* Interleaved and split complex (the interleaved stride in floats, even). */
void vDSP_ctoz(const DSPComplex *c, vDSP_Stride ic, const DSPSplitComplex *z, vDSP_Stride iz, vDSP_Length n);
void vDSP_ztoc(const DSPSplitComplex *z, vDSP_Stride iz, DSPComplex *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_ctozD(const DSPDoubleComplex *c, vDSP_Stride ic, const DSPDoubleSplitComplex *z, vDSP_Stride iz, vDSP_Length n);
void vDSP_ztocD(const DSPDoubleSplitComplex *z, vDSP_Stride iz, DSPDoubleComplex *c, vDSP_Stride ic, vDSP_Length n);

/* Complex vectors: squared magnitudes; products (conjugate -1 takes A's
   conjugate). */
void vDSP_zvmags(const DSPSplitComplex *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_zvmagsD(const DSPDoubleSplitComplex *a, vDSP_Stride ia, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_zvabs(const DSPSplitComplex *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_zvmul(const DSPSplitComplex *a, vDSP_Stride ia, const DSPSplitComplex *b, vDSP_Stride ib,
                const DSPSplitComplex *c, vDSP_Stride ic, vDSP_Length n, int conjugate);
void vDSP_zvmulD(const DSPDoubleSplitComplex *a, vDSP_Stride ia, const DSPDoubleSplitComplex *b, vDSP_Stride ib,
                 const DSPDoubleSplitComplex *c, vDSP_Stride ic, vDSP_Length n, int conjugate);

/* Real vectors. vsub is B - A, as documented. */
void vDSP_vadd(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vaddD(const double *a, vDSP_Stride ia, const double *b, vDSP_Stride ib, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsub(const float *b, vDSP_Stride ib, const float *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsubD(const double *b, vDSP_Stride ib, const double *a, vDSP_Stride ia, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vmul(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vmulD(const double *a, vDSP_Stride ia, const double *b, vDSP_Stride ib, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vdiv(const float *b, vDSP_Stride ib, const float *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsadd(const float *a, vDSP_Stride ia, const float *b, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsaddD(const double *a, vDSP_Stride ia, const double *b, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsmul(const float *a, vDSP_Stride ia, const float *b, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsmulD(const double *a, vDSP_Stride ia, const double *b, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsdiv(const float *a, vDSP_Stride ia, const float *b, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsdivD(const double *a, vDSP_Stride ia, const double *b, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsma(const float *a, vDSP_Stride ia, const float *b, const float *c, vDSP_Stride ic, float *d,
               vDSP_Stride id, vDSP_Length n);
void vDSP_vsmaD(const double *a, vDSP_Stride ia, const double *b, const double *c, vDSP_Stride ic, double *d,
                vDSP_Stride id, vDSP_Length n);
void vDSP_vsmsa(const float *a, vDSP_Stride ia, const float *b, const float *c, float *d, vDSP_Stride id,
                vDSP_Length n);
void vDSP_vsmsaD(const double *a, vDSP_Stride ia, const double *b, const double *c, double *d, vDSP_Stride id,
                 vDSP_Length n);
void vDSP_vma(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, const float *c, vDSP_Stride ic,
              float *d, vDSP_Stride id, vDSP_Length n);
void vDSP_vclip(const float *a, vDSP_Stride ia, const float *low, const float *high, float *c, vDSP_Stride ic,
                vDSP_Length n);
void vDSP_vclipD(const double *a, vDSP_Stride ia, const double *low, const double *high, double *c, vDSP_Stride ic,
                 vDSP_Length n);
void vDSP_vclr(float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vclrD(double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vfill(const float *a, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vfillD(const double *a, double *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vabs(const float *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vneg(const float *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vsq(const float *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vramp(const float *a, const float *b, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vdpsp(const double *a, vDSP_Stride ia, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_vspdp(const float *a, vDSP_Stride ia, double *c, vDSP_Stride ic, vDSP_Length n);

/* Reductions. */
void vDSP_dotpr(const float *a, vDSP_Stride ia, const float *b, vDSP_Stride ib, float *c, vDSP_Length n);
void vDSP_dotprD(const double *a, vDSP_Stride ia, const double *b, vDSP_Stride ib, double *c, vDSP_Length n);
void vDSP_sve(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_svesq(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_svesqD(const double *a, vDSP_Stride ia, double *c, vDSP_Length n);
void vDSP_meanv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_measqv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_rmsqv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_maxv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_minv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_maxmgv(const float *a, vDSP_Stride ia, float *c, vDSP_Length n);
void vDSP_maxmgvD(const double *a, vDSP_Stride ia, double *c, vDSP_Length n);

/* Filters: correlation (convolution with a filter given from its end and
   a negative stride); the two-pole two-zero difference equation, whose
   output's first two values are its initial state. */
void vDSP_conv(const float *a, vDSP_Stride ia, const float *f, vDSP_Stride iff, float *c, vDSP_Stride ic,
               vDSP_Length n, vDSP_Length p);
void vDSP_convD(const double *a, vDSP_Stride ia, const double *f, vDSP_Stride iff, double *c, vDSP_Stride ic,
                vDSP_Length n, vDSP_Length p);
void vDSP_deq22(const float *a, vDSP_Stride ia, const float *b, float *c, vDSP_Stride ic, vDSP_Length n);
void vDSP_deq22D(const double *a, vDSP_Stride ia, const double *b, double *c, vDSP_Stride ic, vDSP_Length n);

#ifdef __cplusplus
}
#endif

#endif
