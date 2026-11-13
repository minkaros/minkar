/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __VTSESSION__
#define __VTSESSION__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

CF_EXTERN_C_BEGIN

typedef CFTypeRef VTSessionRef;
typedef struct OpaqueVTDecompressionSession *VTDecompressionSessionRef;
typedef struct OpaqueVTCompressionSession *VTCompressionSessionRef;

typedef UInt32 VTDecodeFrameFlags;
typedef UInt32 VTDecodeInfoFlags;
typedef UInt32 VTEncodeInfoFlags;

enum {
    kVTPropertyNotSupportedErr = -12900,
    kVTPropertyReadOnlyErr = -12901,
    kVTParameterErr = -12902,
    kVTInvalidSessionErr = -12903,
    kVTAllocationFailedErr = -12904,
    kVTPixelTransferNotSupportedErr = -12905,
    kVTCouldNotFindVideoDecoderErr = -12906,
    kVTCouldNotCreateInstanceErr = -12907,
    kVTCouldNotFindVideoEncoderErr = -12908,
    kVTVideoDecoderBadDataErr = -12909,
    kVTVideoDecoderUnsupportedDataFormatErr = -12910,
    kVTVideoDecoderMalfunctionErr = -12911,
    kVTVideoEncoderMalfunctionErr = -12912,
};

typedef void (*VTDecompressionOutputCallback)(void *decompressionOutputRefCon, void *sourceFrameRefCon,
    OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
    CMTime presentationTimeStamp, CMTime presentationDuration);
typedef struct {
    VTDecompressionOutputCallback decompressionOutputCallback;
    void *decompressionOutputRefCon;
} VTDecompressionOutputCallbackRecord;

typedef void (*VTCompressionOutputCallback)(void *outputCallbackRefCon, void *sourceFrameRefCon,
    OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer);

/* Decoder and encoder specifications. */
CM_EXPORT const CFStringRef kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder;
CM_EXPORT const CFStringRef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder;
CM_EXPORT const CFStringRef kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder;
CM_EXPORT const CFStringRef kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder;
CM_EXPORT const CFStringRef kVTVideoEncoderSpecification_EnableLowLatencyRateControl;

/* Session properties. */
CM_EXPORT const CFStringRef kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_AllowFrameReordering;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_AverageBitRate;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_ConstantBitRate;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_ExpectedFrameRate;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_MaxFrameDelayCount;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_MaxKeyFrameInterval;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_ProfileLevel;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_RealTime;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_ColorPrimaries;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_TransferFunction;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_YCbCrMatrix;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_BaseLayerFrameRateFraction;
CM_EXPORT const CFStringRef kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality;
CM_EXPORT const CFStringRef kVTEncodeFrameOptionKey_ForceKeyFrame;

/* kVTCompressionPropertyKey_ProfileLevel's values. */
CM_EXPORT const CFStringRef kVTProfileLevel_H264_Baseline_AutoLevel;
CM_EXPORT const CFStringRef kVTProfileLevel_H264_Main_AutoLevel;
CM_EXPORT const CFStringRef kVTProfileLevel_H264_High_AutoLevel;

Boolean VTIsHardwareDecodeSupported(CMVideoCodecType codecType);
void VTRegisterSupplementalVideoDecoderIfAvailable(CMVideoCodecType codecType);

OSStatus VTDecompressionSessionCreate(CFAllocatorRef allocator, CMVideoFormatDescriptionRef videoFormatDescription,
    CFDictionaryRef videoDecoderSpecification, CFDictionaryRef destinationImageBufferAttributes,
    const VTDecompressionOutputCallbackRecord *outputCallback, VTDecompressionSessionRef *decompressionSessionOut);
OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session, CMSampleBufferRef sampleBuffer,
    VTDecodeFrameFlags decodeFlags, void *sourceFrameRefCon, VTDecodeInfoFlags *infoFlagsOut);
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef session);
void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session);

OSStatus VTCompressionSessionCreate(CFAllocatorRef allocator, int32_t width, int32_t height, CMVideoCodecType codecType,
    CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes,
    CFAllocatorRef compressedDataAllocator, VTCompressionOutputCallback outputCallback, void *outputCallbackRefCon,
    VTCompressionSessionRef *compressionSessionOut);
OSStatus VTCompressionSessionEncodeFrame(VTCompressionSessionRef session, CVImageBufferRef imageBuffer,
    CMTime presentationTimeStamp, CMTime duration, CFDictionaryRef frameProperties, void *sourceFrameRefcon,
    VTEncodeInfoFlags *infoFlagsOut);
OSStatus VTCompressionSessionCompleteFrames(VTCompressionSessionRef session, CMTime completeUntilPresentationTimeStamp);
void VTCompressionSessionInvalidate(VTCompressionSessionRef session);

OSStatus VTSessionSetProperty(VTSessionRef session, CFStringRef propertyKey, CFTypeRef propertyValue);
OSStatus VTSessionCopyProperty(VTSessionRef session, CFStringRef propertyKey, CFAllocatorRef allocator, void *propertyValueOut);
OSStatus VTSessionCopySupportedPropertyDictionary(VTSessionRef session, CFDictionaryRef *supportedPropertyDictionaryOut);

CF_EXTERN_C_END

#endif
