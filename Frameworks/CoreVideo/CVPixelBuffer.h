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
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGColorSpace.h>

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
extern const CFStringRef kCVImageBufferTransferFunction_SMPTE_ST_428_1;
extern const CFStringRef kCVImageBufferChromaLocation_DV420;
extern const CFStringRef kCVImageBufferCleanApertureKey;
extern const CFStringRef kCVImageBufferCleanApertureWidthKey;
extern const CFStringRef kCVImageBufferCleanApertureHeightKey;
extern const CFStringRef kCVImageBufferCleanApertureHorizontalOffsetKey;
extern const CFStringRef kCVImageBufferCleanApertureVerticalOffsetKey;
extern const CFStringRef kCVImageBufferPreferredCleanApertureKey;
extern const CFStringRef kCVImageBufferFieldCountKey;
extern const CFStringRef kCVImageBufferFieldDetailKey;
extern const CFStringRef kCVImageBufferFieldDetailTemporalTopFirst;
extern const CFStringRef kCVImageBufferFieldDetailTemporalBottomFirst;
extern const CFStringRef kCVImageBufferFieldDetailSpatialFirstLineEarly;
extern const CFStringRef kCVImageBufferFieldDetailSpatialFirstLineLate;
extern const CFStringRef kCVImageBufferPixelAspectRatioKey;
extern const CFStringRef kCVImageBufferPixelAspectRatioHorizontalSpacingKey;
extern const CFStringRef kCVImageBufferPixelAspectRatioVerticalSpacingKey;
extern const CFStringRef kCVImageBufferDisplayDimensionsKey;
extern const CFStringRef kCVImageBufferDisplayWidthKey;
extern const CFStringRef kCVImageBufferDisplayHeightKey;
extern const CFStringRef kCVImageBufferGammaLevelKey;
extern const CFStringRef kCVImageBufferICCProfileKey;
extern const CFStringRef kCVImageBufferChromaSubsamplingKey;
extern const CFStringRef kCVImageBufferChromaSubsampling_420;
extern const CFStringRef kCVImageBufferChromaSubsampling_422;
extern const CFStringRef kCVImageBufferChromaSubsampling_411;
extern const CFStringRef kCVImageBufferAlphaChannelIsOpaque;

extern const CFStringRef kCVBufferMovieTimeKey;
extern const CFStringRef kCVBufferTimeValueKey;
extern const CFStringRef kCVBufferTimeScaleKey;
extern const CFStringRef kCVBufferPropagatedAttachmentsKey;
extern const CFStringRef kCVBufferNonPropagatedAttachmentsKey;

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
extern const CFStringRef kCVPixelBufferMemoryAllocatorKey;
extern const CFStringRef kCVPixelBufferExtendedPixelsLeftKey;
extern const CFStringRef kCVPixelBufferExtendedPixelsTopKey;
extern const CFStringRef kCVPixelBufferExtendedPixelsRightKey;
extern const CFStringRef kCVPixelBufferExtendedPixelsBottomKey;
extern const CFStringRef kCVPixelBufferPlaneAlignmentKey;


typedef void (*CVPixelBufferReleaseBytesCallback)(void *releaseRefCon, const void *baseAddress);
typedef void (*CVPixelBufferReleasePlanarBytesCallback)(void *releaseRefCon, const void *dataPtr, size_t dataSize,
    size_t numberOfPlanes, const void *planeAddresses[]);

/* Pixel formats. */
enum {
    kCVPixelFormatType_1Monochrome = 0x00000001,
    kCVPixelFormatType_2Indexed = 0x00000002,
    kCVPixelFormatType_4Indexed = 0x00000004,
    kCVPixelFormatType_8Indexed = 0x00000008,
    kCVPixelFormatType_1IndexedGray_WhiteIsZero = 0x00000021,
    kCVPixelFormatType_2IndexedGray_WhiteIsZero = 0x00000022,
    kCVPixelFormatType_4IndexedGray_WhiteIsZero = 0x00000024,
    kCVPixelFormatType_8IndexedGray_WhiteIsZero = 0x00000028,
    kCVPixelFormatType_16BE555 = 0x00000010,
    kCVPixelFormatType_16LE555 = 'L555',
    kCVPixelFormatType_16LE5551 = '5551',
    kCVPixelFormatType_16BE565 = 'B565',
    kCVPixelFormatType_16LE565 = 'L565',
    kCVPixelFormatType_24RGB = 0x00000018,
    kCVPixelFormatType_24BGR = '24BG',
    kCVPixelFormatType_32ARGB = 0x00000020,
    kCVPixelFormatType_32BGRA = 'BGRA',
    kCVPixelFormatType_32ABGR = 'ABGR',
    kCVPixelFormatType_32RGBA = 'RGBA',
    kCVPixelFormatType_64ARGB = 'b64a',
    kCVPixelFormatType_64RGBALE = 'l64r',
    kCVPixelFormatType_48RGB = 'b48r',
    kCVPixelFormatType_32AlphaGray = 'b32a',
    kCVPixelFormatType_16Gray = 'b16g',
    kCVPixelFormatType_30RGB = 'R10k',
    kCVPixelFormatType_422YpCbCr8 = '2vuy',
    kCVPixelFormatType_4444YpCbCrA8 = 'v408',
    kCVPixelFormatType_4444YpCbCrA8R = 'r408',
    kCVPixelFormatType_4444AYpCbCr8 = 'y408',
    kCVPixelFormatType_4444AYpCbCr16 = 'y416',
    kCVPixelFormatType_444YpCbCr8 = 'v308',
    kCVPixelFormatType_422YpCbCr16 = 'v216',
    kCVPixelFormatType_422YpCbCr10 = 'v210',
    kCVPixelFormatType_444YpCbCr10 = 'v410',
    kCVPixelFormatType_420YpCbCr8Planar = 'y420',
    kCVPixelFormatType_420YpCbCr8PlanarFullRange = 'f420',
    kCVPixelFormatType_422YpCbCr_4A_8BiPlanar = 'a2vy',
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange = '420v',
    kCVPixelFormatType_420YpCbCr8BiPlanarFullRange = '420f',
    kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange = '422v',
    kCVPixelFormatType_422YpCbCr8BiPlanarFullRange = '422f',
    kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange = '444v',
    kCVPixelFormatType_444YpCbCr8BiPlanarFullRange = '444f',
    kCVPixelFormatType_422YpCbCr8_yuvs = 'yuvs',
    kCVPixelFormatType_422YpCbCr8FullRange = 'yuvf',
    kCVPixelFormatType_OneComponent8 = 'L008',
    kCVPixelFormatType_TwoComponent8 = '2C08',
    kCVPixelFormatType_30RGBLEPackedWideGamut = 'w30r',
    kCVPixelFormatType_ARGB2101010LEPacked = 'l10r',
    kCVPixelFormatType_OneComponent16Half = 'L00h',
    kCVPixelFormatType_OneComponent32Float = 'L00f',
    kCVPixelFormatType_TwoComponent16Half = '2C0h',
    kCVPixelFormatType_TwoComponent32Float = '2C0f',
    kCVPixelFormatType_64RGBAHalf = 'RGhA',
    kCVPixelFormatType_128RGBAFloat = 'RGfA',
    kCVPixelFormatType_14Bayer_GRBG = 'grb4',
    kCVPixelFormatType_14Bayer_RGGB = 'rgg4',
    kCVPixelFormatType_14Bayer_BGGR = 'bgg4',
    kCVPixelFormatType_14Bayer_GBRG = 'gbr4',
    kCVPixelFormatType_DisparityFloat16 = 'hdis',
    kCVPixelFormatType_DisparityFloat32 = 'fdis',
    kCVPixelFormatType_DepthFloat16 = 'hdep',
    kCVPixelFormatType_DepthFloat32 = 'fdep',
    kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420',
    kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange = 'x422',
    kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange = 'x444',
    kCVPixelFormatType_420YpCbCr10BiPlanarFullRange = 'xf20',
    kCVPixelFormatType_422YpCbCr10BiPlanarFullRange = 'xf22',
    kCVPixelFormatType_444YpCbCr10BiPlanarFullRange = 'xf44',
    kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar = 'v0a8',
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

/* An image buffer's sizes, clean rectangle and color space, from its
   attachments. */
CGSize CVImageBufferGetEncodedSize(CVImageBufferRef imageBuffer);
CGSize CVImageBufferGetDisplaySize(CVImageBufferRef imageBuffer);
CGRect CVImageBufferGetCleanRect(CVImageBufferRef imageBuffer);
Boolean CVImageBufferIsFlipped(CVImageBufferRef imageBuffer);
CGColorSpaceRef CVImageBufferGetColorSpace(CVImageBufferRef imageBuffer);
CGColorSpaceRef CVImageBufferCreateColorSpaceFromAttachments(CFDictionaryRef attachments);

CVReturn CVPixelBufferCreateResolvedAttributesDictionary(CFAllocatorRef allocator, CFArrayRef attributes,
    CFDictionaryRef *resolvedDictionaryOut);
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
