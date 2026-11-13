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
#include <IOSurface/IOSurfaceRef.h>

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


typedef void (*CVPixelBufferReleaseBytesCallback)(void *releaseRefCon, const void *baseAddress);
typedef void (*CVPixelBufferReleasePlanarBytesCallback)(void *releaseRefCon, const void *dataPtr, size_t dataSize,
    size_t numberOfPlanes, const void *planeAddresses[]);

/* Pixel formats. */
enum {
    kCVPixelFormatType_32ARGB = 0x00000020,
    kCVPixelFormatType_32BGRA = 'BGRA',
    kCVPixelFormatType_32RGBA = 'RGBA',
    kCVPixelFormatType_24RGB = 0x00000018,
    kCVPixelFormatType_OneComponent8 = 'L008',
    kCVPixelFormatType_422YpCbCr8 = '2vuy',
    kCVPixelFormatType_422YpCbCr8_yuvs = 'yuvs',
    kCVPixelFormatType_420YpCbCr8Planar = 'y420',
    kCVPixelFormatType_420YpCbCr8PlanarFullRange = 'f420',
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange = '420v',
    kCVPixelFormatType_420YpCbCr8BiPlanarFullRange = '420f',
    kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420',
    kCVPixelFormatType_420YpCbCr10BiPlanarFullRange = 'xf20',
    kCVPixelFormatType_64RGBAHalf = 'RGhA',
};

enum {
    kCVPixelBufferLock_ReadOnly = 0x00000001
};

CFTypeID CVPixelBufferGetTypeID(void);

/* Buffers: retained and released as CF objects; their attachments. */
CVBufferRef CVBufferRetain(CVBufferRef buffer);
void CVBufferRelease(CVBufferRef buffer);
void CVBufferSetAttachment(CVBufferRef buffer, CFStringRef key, CFTypeRef value, CVAttachmentMode attachmentMode);
CFTypeRef CVBufferGetAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *attachmentMode);
CFTypeRef CVBufferCopyAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *attachmentMode);
void CVBufferRemoveAttachment(CVBufferRef buffer, CFStringRef key);
void CVBufferRemoveAllAttachments(CVBufferRef buffer);
CFDictionaryRef CVBufferGetAttachments(CVBufferRef buffer, CVAttachmentMode attachmentMode);
CFDictionaryRef CVBufferCopyAttachments(CVBufferRef buffer, CVAttachmentMode attachmentMode);
void CVBufferSetAttachments(CVBufferRef buffer, CFDictionaryRef theAttachments, CVAttachmentMode attachmentMode);
void CVBufferPropagateAttachments(CVBufferRef sourceBuffer, CVBufferRef destinationBuffer);

CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef pixelBuffer);
void CVPixelBufferRelease(CVPixelBufferRef pixelBuffer);
CVReturn CVPixelBufferCreate(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
    CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
CVReturn CVPixelBufferCreateWithBytes(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
    void *baseAddress, size_t bytesPerRow, CVPixelBufferReleaseBytesCallback releaseCallback, void *releaseRefCon,
    CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
CVReturn CVPixelBufferCreateWithPlanarBytes(CFAllocatorRef allocator, size_t width, size_t height,
    OSType pixelFormatType, void *dataPtr, size_t dataSize, size_t numberOfPlanes, void *planeBaseAddress[],
    size_t planeWidth[], size_t planeHeight[], size_t planeBytesPerRow[],
    CVPixelBufferReleasePlanarBytesCallback releaseCallback, void *releaseRefCon,
    CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
CVReturn CVPixelBufferCreateWithIOSurface(CFAllocatorRef allocator, IOSurfaceRef surface,
    CFDictionaryRef pixelBufferAttributes, CVPixelBufferRef *pixelBufferOut);
IOSurfaceRef CVPixelBufferGetIOSurface(CVPixelBufferRef pixelBuffer);

CVReturn CVPixelBufferLockBaseAddress(CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags);
CVReturn CVPixelBufferUnlockBaseAddress(CVPixelBufferRef pixelBuffer, CVOptionFlags unlockFlags);
size_t CVPixelBufferGetWidth(CVPixelBufferRef pixelBuffer);
size_t CVPixelBufferGetHeight(CVPixelBufferRef pixelBuffer);
OSType CVPixelBufferGetPixelFormatType(CVPixelBufferRef pixelBuffer);
void *CVPixelBufferGetBaseAddress(CVPixelBufferRef pixelBuffer);
size_t CVPixelBufferGetBytesPerRow(CVPixelBufferRef pixelBuffer);
size_t CVPixelBufferGetDataSize(CVPixelBufferRef pixelBuffer);
Boolean CVPixelBufferIsPlanar(CVPixelBufferRef pixelBuffer);
size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef pixelBuffer);
size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex);
size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex);
void *CVPixelBufferGetBaseAddressOfPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex);
size_t CVPixelBufferGetBytesPerRowOfPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex);
void CVPixelBufferGetExtendedPixels(CVPixelBufferRef pixelBuffer, size_t *extraColumnsOnLeft,
    size_t *extraColumnsOnRight, size_t *extraRowsOnTop, size_t *extraRowsOnBottom);
CVReturn CVPixelBufferFillExtendedPixels(CVPixelBufferRef pixelBuffer);

#ifdef __cplusplus
}
#endif

#endif
