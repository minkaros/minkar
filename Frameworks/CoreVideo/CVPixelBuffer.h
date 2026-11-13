/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Buffers of pixels (video frames, images): CVBuffer (attachments),
 * CVImageBuffer (an image's color description) and CVPixelBuffer (the
 * pixels, in planes, in memory or an IOSurface).
 */

#ifndef __CVPIXELBUFFER__
#define __CVPIXELBUFFER__

#include <CoreFoundation/CoreFoundation.h>
#import <CoreVideo/CVBase.h>
#import <CoreVideo/CVReturn.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __CVBuffer *CVBufferRef;
typedef CVBufferRef CVImageBufferRef;
typedef CVImageBufferRef CVPixelBufferRef;

typedef enum {
    kCVAttachmentMode_ShouldNotPropagate = 0,
    kCVAttachmentMode_ShouldPropagate = 1
} CVAttachmentMode;

/* Image buffer attachments: what the pixels mean. */
extern const CFStringRef kCVImageBufferCGColorSpaceKey;
extern const CFStringRef kCVImageBufferColorPrimariesKey;
extern const CFStringRef kCVImageBufferColorPrimaries_ITU_R_709_2;
extern const CFStringRef kCVImageBufferColorPrimaries_EBU_3213;
extern const CFStringRef kCVImageBufferColorPrimaries_SMPTE_C;
extern const CFStringRef kCVImageBufferColorPrimaries_P22;
extern const CFStringRef kCVImageBufferColorPrimaries_DCI_P3;
extern const CFStringRef kCVImageBufferColorPrimaries_P3_D65;
extern const CFStringRef kCVImageBufferColorPrimaries_ITU_R_2020;
extern const CFStringRef kCVImageBufferTransferFunctionKey;
extern const CFStringRef kCVImageBufferTransferFunction_ITU_R_709_2;
extern const CFStringRef kCVImageBufferTransferFunction_SMPTE_240M_1995;
extern const CFStringRef kCVImageBufferTransferFunction_UseGamma;
extern const CFStringRef kCVImageBufferTransferFunction_sRGB;
extern const CFStringRef kCVImageBufferTransferFunction_Linear;
extern const CFStringRef kCVImageBufferTransferFunction_ITU_R_2020;
extern const CFStringRef kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;
extern const CFStringRef kCVImageBufferTransferFunction_ITU_R_2100_HLG;
extern const CFStringRef kCVImageBufferYCbCrMatrixKey;
extern const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_709_2;
extern const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_601_4;
extern const CFStringRef kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
extern const CFStringRef kCVImageBufferYCbCrMatrix_ITU_R_2020;
extern const CFStringRef kCVImageBufferChromaLocationTopFieldKey;
extern const CFStringRef kCVImageBufferChromaLocationBottomFieldKey;
extern const CFStringRef kCVImageBufferChromaLocation_Left;
extern const CFStringRef kCVImageBufferChromaLocation_Center;
extern const CFStringRef kCVImageBufferChromaLocation_TopLeft;
extern const CFStringRef kCVImageBufferChromaLocation_Top;
extern const CFStringRef kCVImageBufferChromaLocation_BottomLeft;
extern const CFStringRef kCVImageBufferChromaLocation_Bottom;
extern const CFStringRef kCVImageBufferMasteringDisplayColorVolumeKey;
extern const CFStringRef kCVImageBufferContentLightLevelInfoKey;

/* Pixel buffer attributes (CVPixelBufferCreate's dictionary). */
extern const CFStringRef kCVPixelBufferPixelFormatTypeKey;
extern const CFStringRef kCVPixelBufferWidthKey;
extern const CFStringRef kCVPixelBufferHeightKey;
extern const CFStringRef kCVPixelBufferBytesPerRowAlignmentKey;
extern const CFStringRef kCVPixelBufferIOSurfacePropertiesKey;
extern const CFStringRef kCVPixelBufferOpenGLCompatibilityKey;
extern const CFStringRef kCVPixelBufferMetalCompatibilityKey;
extern const CFStringRef kCVPixelBufferCGImageCompatibilityKey;
extern const CFStringRef kCVPixelBufferCGBitmapContextCompatibilityKey;

#ifdef __cplusplus
}
#endif

#endif
