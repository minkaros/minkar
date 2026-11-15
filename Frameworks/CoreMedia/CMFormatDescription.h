/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Format descriptions: what a track's samples are (their media type and
 * subtype, with extensions), with an audio stream's basic description,
 * channel layout and magic cookie, or a video stream's dimensions and its
 * codec's configuration (H.264's and HEVC's parameter sets).
 */

#ifndef __CMFORMATDESCRIPTION__
#define __CMFORMATDESCRIPTION__

#include <CoreMedia/CMBase.h>
#include <CoreMedia/CMTime.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioFormat.h>
#include <CoreGraphics/CGGeometry.h>
#include <CoreVideo/CVPixelBuffer.h>

enum {
    kCMFormatDescriptionError_InvalidParameter = -12710,
    kCMFormatDescriptionError_AllocationFailed = -12711,
    kCMFormatDescriptionError_ValueNotAvailable = -12718,
};

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

typedef FourCharCode CMMediaType;
enum {
    kCMMediaType_Video = 'vide',
    kCMMediaType_Audio = 'soun',
    kCMMediaType_Muxed = 'muxx',
    kCMMediaType_Text = 'text',
    kCMMediaType_ClosedCaption = 'clcp',
    kCMMediaType_Subtitle = 'sbtl',
    kCMMediaType_TimeCode = 'tmcd',
    kCMMediaType_Metadata = 'meta',
};

CM_EXPORT OSStatus CMFormatDescriptionCreate(CFAllocatorRef allocator, CMMediaType mediaType, FourCharCode mediaSubType,
                                             CFDictionaryRef extensions, CMFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT CFTypeID CMFormatDescriptionGetTypeID(void);
CM_EXPORT Boolean CMFormatDescriptionEqual(CMFormatDescriptionRef formatDescription,
                                           CMFormatDescriptionRef otherFormatDescription);
CM_EXPORT Boolean CMFormatDescriptionEqualIgnoringExtensionKeys(CMFormatDescriptionRef formatDescription,
                                                                CMFormatDescriptionRef otherFormatDescription,
                                                                CFTypeRef formatDescriptionExtensionKeysToIgnore,
                                                                CFTypeRef sampleDescriptionExtensionAtomKeysToIgnore);
CM_EXPORT CMMediaType CMFormatDescriptionGetMediaType(CMFormatDescriptionRef desc);
CM_EXPORT FourCharCode CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef desc);
CM_EXPORT CFDictionaryRef CMFormatDescriptionGetExtensions(CMFormatDescriptionRef desc);
CM_EXPORT CFPropertyListRef CMFormatDescriptionGetExtension(CMFormatDescriptionRef desc, CFStringRef extensionKey);

CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_OriginalCompressionSettings;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_VerbatimSampleDescription;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_VerbatimISOSampleEntry;

/* ---- Audio ---- */

typedef CMFormatDescriptionRef CMAudioFormatDescriptionRef;

typedef FourCharCode CMAudioCodecType;
enum {
    kCMAudioCodecType_AAC_LCProtected = 'paac',
    kCMAudioCodecType_AAC_AudibleProtected = 'aaac',
};

CM_EXPORT OSStatus CMAudioFormatDescriptionCreate(CFAllocatorRef allocator, const AudioStreamBasicDescription *asbd,
                                                  size_t layoutSize, const AudioChannelLayout *layout,
                                                  size_t magicCookieSize, const void *magicCookie,
                                                  CFDictionaryRef extensions,
                                                  CMAudioFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT const AudioStreamBasicDescription *
CMAudioFormatDescriptionGetStreamBasicDescription(CMAudioFormatDescriptionRef desc);
CM_EXPORT const void *CMAudioFormatDescriptionGetMagicCookie(CMAudioFormatDescriptionRef desc,
                                                             size_t *sizeOut);
CM_EXPORT const AudioChannelLayout *CMAudioFormatDescriptionGetChannelLayout(CMAudioFormatDescriptionRef desc,
                                                                            size_t *sizeOut);
CM_EXPORT const AudioFormatListItem *CMAudioFormatDescriptionGetFormatList(CMAudioFormatDescriptionRef desc,
                                                                          size_t *sizeOut);
CM_EXPORT const AudioFormatListItem *CMAudioFormatDescriptionGetRichestDecodableFormat(CMAudioFormatDescriptionRef desc);
CM_EXPORT const AudioFormatListItem *CMAudioFormatDescriptionGetMostCompatibleFormat(CMAudioFormatDescriptionRef desc);

typedef uint32_t CMAudioFormatDescriptionMask;
enum {
    kCMAudioFormatDescriptionMask_StreamBasicDescription = (1 << 0),
    kCMAudioFormatDescriptionMask_MagicCookie = (1 << 1),
    kCMAudioFormatDescriptionMask_ChannelLayout = (1 << 2),
    kCMAudioFormatDescriptionMask_Extensions = (1 << 3),
    kCMAudioFormatDescriptionMask_All = 0xF,
};

CM_EXPORT Boolean CMAudioFormatDescriptionEqual(CMAudioFormatDescriptionRef formatDescription,
                                                CMAudioFormatDescriptionRef otherFormatDescription,
                                                CMAudioFormatDescriptionMask equalityMask,
                                                CMAudioFormatDescriptionMask *equalityMaskOut);

/* ---- Video ---- */

typedef CMFormatDescriptionRef CMVideoFormatDescriptionRef;

typedef FourCharCode CMPixelFormatType;
enum {
    kCMPixelFormat_32ARGB = 32,
    kCMPixelFormat_32BGRA = 'BGRA',
    kCMPixelFormat_24RGB = 24,
    kCMPixelFormat_16BE555 = 16,
    kCMPixelFormat_16BE565 = 'B565',
    kCMPixelFormat_16LE555 = 'L555',
    kCMPixelFormat_16LE565 = 'L565',
    kCMPixelFormat_16LE5551 = '5551',
    kCMPixelFormat_422YpCbCr8 = '2vuy',
    kCMPixelFormat_422YpCbCr8_yuvs = 'yuvs',
    kCMPixelFormat_444YpCbCr8 = 'v308',
    kCMPixelFormat_4444YpCbCrA8 = 'v408',
    kCMPixelFormat_422YpCbCr16 = 'v216',
    kCMPixelFormat_422YpCbCr10 = 'v210',
    kCMPixelFormat_444YpCbCr10 = 'v410',
    kCMPixelFormat_8IndexedGray_WhiteIsZero = 0x00000028,
};

typedef FourCharCode CMVideoCodecType;
enum {
    kCMVideoCodecType_422YpCbCr8 = kCMPixelFormat_422YpCbCr8,
    kCMVideoCodecType_Animation = 'rle ',
    kCMVideoCodecType_Cinepak = 'cvid',
    kCMVideoCodecType_JPEG = 'jpeg',
    kCMVideoCodecType_JPEG_OpenDML = 'dmb1',
    kCMVideoCodecType_SorensonVideo = 'SVQ1',
    kCMVideoCodecType_SorensonVideo3 = 'SVQ3',
    kCMVideoCodecType_H263 = 'h263',
    kCMVideoCodecType_H264 = 'avc1',
    kCMVideoCodecType_HEVC = 'hvc1',
    kCMVideoCodecType_HEVCWithAlpha = 'muxa',
    kCMVideoCodecType_MPEG4Video = 'mp4v',
    kCMVideoCodecType_MPEG2Video = 'mp2v',
    kCMVideoCodecType_MPEG1Video = 'mp1v',
    kCMVideoCodecType_VP9 = 'vp09',
    kCMVideoCodecType_AV1 = 'av01',
    kCMVideoCodecType_DVCNTSC = 'dvc ',
    kCMVideoCodecType_DVCPAL = 'dvcp',
    kCMVideoCodecType_DVCProPAL = 'dvpp',
    kCMVideoCodecType_DVCPro50NTSC = 'dv5n',
    kCMVideoCodecType_DVCPro50PAL = 'dv5p',
    kCMVideoCodecType_DVCPROHD720p60 = 'dvhp',
    kCMVideoCodecType_DVCPROHD720p50 = 'dvhq',
    kCMVideoCodecType_DVCPROHD1080i60 = 'dvh6',
    kCMVideoCodecType_DVCPROHD1080i50 = 'dvh5',
    kCMVideoCodecType_DVCPROHD1080p30 = 'dvh3',
    kCMVideoCodecType_DVCPROHD1080p25 = 'dvh2',
    kCMVideoCodecType_AppleProRes4444XQ = 'ap4x',
    kCMVideoCodecType_AppleProRes4444 = 'ap4h',
    kCMVideoCodecType_AppleProRes422HQ = 'apch',
    kCMVideoCodecType_AppleProRes422 = 'apcn',
    kCMVideoCodecType_AppleProRes422LT = 'apcs',
    kCMVideoCodecType_AppleProRes422Proxy = 'apco',
    kCMVideoCodecType_AppleProResRAW = 'aprn',
    kCMVideoCodecType_AppleProResRAWHQ = 'aprh',
};

typedef struct {
    int32_t width;
    int32_t height;
} CMVideoDimensions;

CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_FormatName;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_Depth;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_CleanAperture;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureWidth;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureHeight;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureHorizontalOffset;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureVerticalOffset;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureWidthRational;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureHeightRational;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureHorizontalOffsetRational;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_CleanApertureVerticalOffsetRational;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_FieldCount;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_FieldDetail;
CM_EXPORT const CFStringRef kCMFormatDescriptionFieldDetail_TemporalTopFirst;
CM_EXPORT const CFStringRef kCMFormatDescriptionFieldDetail_TemporalBottomFirst;
CM_EXPORT const CFStringRef kCMFormatDescriptionFieldDetail_SpatialFirstLineEarly;
CM_EXPORT const CFStringRef kCMFormatDescriptionFieldDetail_SpatialFirstLineLate;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_PixelAspectRatio;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_PixelAspectRatioHorizontalSpacing;
CM_EXPORT const CFStringRef kCMFormatDescriptionKey_PixelAspectRatioVerticalSpacing;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ColorPrimaries;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_ITU_R_709_2;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_EBU_3213;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_SMPTE_C;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_DCI_P3;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_P3_D65;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_ITU_R_2020;
CM_EXPORT const CFStringRef kCMFormatDescriptionColorPrimaries_P22;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_TransferFunction;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_ITU_R_709_2;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_SMPTE_240M_1995;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_UseGamma;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_ITU_R_2020;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_Linear;
CM_EXPORT const CFStringRef kCMFormatDescriptionTransferFunction_sRGB;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_GammaLevel;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_YCbCrMatrix;
CM_EXPORT const CFStringRef kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2;
CM_EXPORT const CFStringRef kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4;
CM_EXPORT const CFStringRef kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995;
CM_EXPORT const CFStringRef kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_FullRangeVideo;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ICCProfile;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_BytesPerRow;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ChromaLocationTopField;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ChromaLocationBottomField;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_Left;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_Center;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_TopLeft;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_Top;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_BottomLeft;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_Bottom;
CM_EXPORT const CFStringRef kCMFormatDescriptionChromaLocation_DV420;
CM_EXPORT const CFStringRef kCMFormatDescriptionConformsToMPEG2VideoProfile;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_TemporalQuality;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_SpatialQuality;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_VerbatimImageDescription;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_Version;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_RevisionLevel;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_Vendor;
CM_EXPORT const CFStringRef kCMFormatDescriptionVendor_Apple;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_MasteringDisplayColorVolume;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ContentLightLevelInfo;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_AlternativeTransferCharacteristics;

CM_EXPORT OSStatus CMVideoFormatDescriptionCreate(CFAllocatorRef allocator, CMVideoCodecType codecType, int32_t width,
                                                  int32_t height, CFDictionaryRef extensions,
                                                  CMVideoFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef videoDesc);
CM_EXPORT CGSize CMVideoFormatDescriptionGetPresentationDimensions(CMVideoFormatDescriptionRef videoDesc,
                                                                   Boolean usePixelAspectRatio,
                                                                   Boolean useCleanAperture);
CM_EXPORT CGRect CMVideoFormatDescriptionGetCleanAperture(CMVideoFormatDescriptionRef videoDesc,
                                                          Boolean originIsAtTopLeft);
CM_EXPORT CFArrayRef CMVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers(void);
CM_EXPORT OSStatus CMVideoFormatDescriptionCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                                CMVideoFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT Boolean CMVideoFormatDescriptionMatchesImageBuffer(CMVideoFormatDescriptionRef videoDesc,
                                                             CVImageBufferRef imageBuffer);

CM_EXPORT OSStatus CMVideoFormatDescriptionCreateFromH264ParameterSets(
    CFAllocatorRef allocator, size_t parameterSetCount, const uint8_t *const *parameterSetPointers,
    const size_t *parameterSetSizes, int NALUnitHeaderLength, CMFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT OSStatus CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef videoDesc,
                                                                      size_t parameterSetIndex,
                                                                      const uint8_t **parameterSetPointerOut,
                                                                      size_t *parameterSetSizeOut,
                                                                      size_t *parameterSetCountOut,
                                                                      int *NALUnitHeaderLengthOut);
CM_EXPORT OSStatus CMVideoFormatDescriptionCreateFromHEVCParameterSets(
    CFAllocatorRef allocator, size_t parameterSetCount, const uint8_t *const *parameterSetPointers,
    const size_t *parameterSetSizes, int NALUnitHeaderLength, CFDictionaryRef extensions,
    CMFormatDescriptionRef *formatDescriptionOut);
CM_EXPORT OSStatus CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(CMFormatDescriptionRef videoDesc,
                                                                      size_t parameterSetIndex,
                                                                      const uint8_t **parameterSetPointerOut,
                                                                      size_t *parameterSetSizeOut,
                                                                      size_t *parameterSetCountOut,
                                                                      int *NALUnitHeaderLengthOut);

#endif
