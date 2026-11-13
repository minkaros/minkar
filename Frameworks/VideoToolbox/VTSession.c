/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * No decoder or encoder is registered yet, so no session can be made.
 * A key's value is its name after "Key_" or "Specification_".
 */

#include "VTSession.h"

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder, "EnableHardwareAcceleratedVideoDecoder");
KEY(kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder, "RequireHardwareAcceleratedVideoDecoder");
KEY(kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, "EnableHardwareAcceleratedVideoEncoder");
KEY(kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder, "RequireHardwareAcceleratedVideoEncoder");
KEY(kVTVideoEncoderSpecification_EnableLowLatencyRateControl, "EnableLowLatencyRateControl");
KEY(kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, "UsingHardwareAcceleratedVideoDecoder");
KEY(kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, "UsingHardwareAcceleratedVideoEncoder");
KEY(kVTCompressionPropertyKey_AllowFrameReordering, "AllowFrameReordering");
KEY(kVTCompressionPropertyKey_AverageBitRate, "AverageBitRate");
KEY(kVTCompressionPropertyKey_ConstantBitRate, "ConstantBitRate");
KEY(kVTCompressionPropertyKey_ExpectedFrameRate, "ExpectedFrameRate");
KEY(kVTCompressionPropertyKey_MaxFrameDelayCount, "MaxFrameDelayCount");
KEY(kVTCompressionPropertyKey_MaxKeyFrameInterval, "MaxKeyFrameInterval");
KEY(kVTCompressionPropertyKey_ProfileLevel, "ProfileLevel");
KEY(kVTCompressionPropertyKey_RealTime, "RealTime");
KEY(kVTCompressionPropertyKey_ColorPrimaries, "ColorPrimaries");
KEY(kVTCompressionPropertyKey_TransferFunction, "TransferFunction");
KEY(kVTCompressionPropertyKey_YCbCrMatrix, "YCbCrMatrix");
KEY(kVTCompressionPropertyKey_BaseLayerFrameRateFraction, "BaseLayerFrameRateFraction");
KEY(kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality, "PrioritizeEncodingSpeedOverQuality");
KEY(kVTEncodeFrameOptionKey_ForceKeyFrame, "ForceKeyFrame");
KEY(kVTProfileLevel_H264_Baseline_AutoLevel, "H264_Baseline_AutoLevel");
KEY(kVTProfileLevel_H264_Main_AutoLevel, "H264_Main_AutoLevel");
KEY(kVTProfileLevel_H264_High_AutoLevel, "H264_High_AutoLevel");

Boolean VTIsHardwareDecodeSupported(CMVideoCodecType codecType) {
    return false;
}

void VTRegisterSupplementalVideoDecoderIfAvailable(CMVideoCodecType codecType) {
}

OSStatus VTDecompressionSessionCreate(CFAllocatorRef allocator, CMVideoFormatDescriptionRef format,
    CFDictionaryRef decoderSpecification, CFDictionaryRef imageBufferAttributes,
    const VTDecompressionOutputCallbackRecord *outputCallback, VTDecompressionSessionRef *sessionOut) {
    if (sessionOut != NULL)
        *sessionOut = NULL;
    return kVTCouldNotFindVideoDecoderErr;
}

OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session, CMSampleBufferRef sampleBuffer,
    VTDecodeFrameFlags flags, void *refCon, VTDecodeInfoFlags *infoFlagsOut) {
    return kVTInvalidSessionErr;
}

OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef session) {
    return kVTInvalidSessionErr;
}

void VTDecompressionSessionInvalidate(VTDecompressionSessionRef session) {
}

OSStatus VTCompressionSessionCreate(CFAllocatorRef allocator, int32_t width, int32_t height, CMVideoCodecType codecType,
    CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes,
    CFAllocatorRef compressedDataAllocator, VTCompressionOutputCallback outputCallback, void *refCon,
    VTCompressionSessionRef *sessionOut) {
    if (sessionOut != NULL)
        *sessionOut = NULL;
    return kVTCouldNotFindVideoEncoderErr;
}

OSStatus VTCompressionSessionEncodeFrame(VTCompressionSessionRef session, CVImageBufferRef imageBuffer,
    CMTime pts, CMTime duration, CFDictionaryRef frameProperties, void *refCon, VTEncodeInfoFlags *infoFlagsOut) {
    return kVTInvalidSessionErr;
}

OSStatus VTCompressionSessionCompleteFrames(VTCompressionSessionRef session, CMTime until) {
    return kVTInvalidSessionErr;
}

void VTCompressionSessionInvalidate(VTCompressionSessionRef session) {
}

OSStatus VTSessionSetProperty(VTSessionRef session, CFStringRef key, CFTypeRef value) {
    return kVTInvalidSessionErr;
}

OSStatus VTSessionCopyProperty(VTSessionRef session, CFStringRef key, CFAllocatorRef allocator, void *valueOut) {
    return kVTInvalidSessionErr;
}

OSStatus VTSessionCopySupportedPropertyDictionary(VTSessionRef session, CFDictionaryRef *dictionaryOut) {
    return kVTInvalidSessionErr;
}
