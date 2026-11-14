/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The types shared by the audio frameworks: sample formats, buffers and
 * time stamps.
 */

#ifndef __COREAUDIOTYPES__
#define __COREAUDIOTYPES__

#include <MacTypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Errors any audio call may return. */
enum {
    kAudio_UnimplementedError = -4,
    kAudio_FileNotFoundError = -43,
    kAudio_FilePermissionError = -54,
    kAudio_TooManyFilesOpenError = -42,
    kAudio_BadFilePathError = '!pth',
    kAudio_ParamError = -50,
    kAudio_MemFullError = -108,
};

typedef Float64 AudioSampleType_Float64;
typedef UInt32 AudioFormatID;
typedef UInt32 AudioFormatFlags;
typedef UInt32 AudioChannelLabel;
typedef UInt32 AudioChannelLayoutTag;

struct AudioValueRange {
    Float64 mMinimum;
    Float64 mMaximum;
};
typedef struct AudioValueRange AudioValueRange;

struct AudioValueTranslation {
    void *mInputData;
    UInt32 mInputDataSize;
    void *mOutputData;
    UInt32 mOutputDataSize;
};
typedef struct AudioValueTranslation AudioValueTranslation;

struct AudioBuffer {
    UInt32 mNumberChannels;
    UInt32 mDataByteSize;
    void *mData;
};
typedef struct AudioBuffer AudioBuffer;

struct AudioBufferList {
    UInt32 mNumberBuffers;
    AudioBuffer mBuffers[1];
};
typedef struct AudioBufferList AudioBufferList;

struct AudioStreamBasicDescription {
    Float64 mSampleRate;
    AudioFormatID mFormatID;
    AudioFormatFlags mFormatFlags;
    UInt32 mBytesPerPacket;
    UInt32 mFramesPerPacket;
    UInt32 mBytesPerFrame;
    UInt32 mChannelsPerFrame;
    UInt32 mBitsPerChannel;
    UInt32 mReserved;
};
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;

struct AudioStreamPacketDescription {
    SInt64 mStartOffset;
    UInt32 mVariableFramesInPacket;
    UInt32 mDataByteSize;
};
typedef struct AudioStreamPacketDescription AudioStreamPacketDescription;

enum {
    kAudioFormatLinearPCM = 'lpcm',
    kAudioFormatAC3 = 'ac-3',
    kAudioFormat60958AC3 = 'cac3',
    kAudioFormatAppleIMA4 = 'ima4',
    kAudioFormatMPEG4AAC = 'aac ',
    kAudioFormatMPEG4CELP = 'celp',
    kAudioFormatMPEG4HVXC = 'hvxc',
    kAudioFormatMPEG4TwinVQ = 'twvq',
    kAudioFormatMACE3 = 'MAC3',
    kAudioFormatMACE6 = 'MAC6',
    kAudioFormatULaw = 'ulaw',
    kAudioFormatALaw = 'alaw',
    kAudioFormatQDesign = 'QDMC',
    kAudioFormatQDesign2 = 'QDM2',
    kAudioFormatQUALCOMM = 'Qclp',
    kAudioFormatMPEGLayer1 = '.mp1',
    kAudioFormatMPEGLayer2 = '.mp2',
    kAudioFormatMPEGLayer3 = '.mp3',
    kAudioFormatTimeCode = 'time',
    kAudioFormatMIDIStream = 'midi',
    kAudioFormatParameterValueStream = 'apvs',
    kAudioFormatAppleLossless = 'alac',
    kAudioFormatMPEG4AAC_HE = 'aach',
    kAudioFormatMPEG4AAC_LD = 'aacl',
    kAudioFormatMPEG4AAC_ELD = 'aace',
    kAudioFormatMPEG4AAC_ELD_SBR = 'aacf',
    kAudioFormatMPEG4AAC_ELD_V2 = 'aacg',
    kAudioFormatMPEG4AAC_HE_V2 = 'aacp',
    kAudioFormatMPEG4AAC_Spatial = 'aacs',
    kAudioFormatMPEGD_USAC = 'usac',
    kAudioFormatAMR = 'samr',
    kAudioFormatAMR_WB = 'sawb',
    kAudioFormatAudible = 'AUDB',
    kAudioFormatiLBC = 'ilbc',
    kAudioFormatDVIIntelIMA = 0x6D730011,
    kAudioFormatMicrosoftGSM = 0x6D730031,
    kAudioFormatAES3 = 'aes3',
    kAudioFormatEnhancedAC3 = 'ec-3',
    kAudioFormatFLAC = 'flac',
    kAudioFormatOpus = 'opus',
};

enum {
    kAudioFormatFlagIsFloat = (1U << 0),
    kAudioFormatFlagIsBigEndian = (1U << 1),
    kAudioFormatFlagIsSignedInteger = (1U << 2),
    kAudioFormatFlagIsPacked = (1U << 3),
    kAudioFormatFlagIsAlignedHigh = (1U << 4),
    kAudioFormatFlagIsNonInterleaved = (1U << 5),
    kAudioFormatFlagIsNonMixable = (1U << 6),
    kLinearPCMFormatFlagIsFloat = kAudioFormatFlagIsFloat,
    kLinearPCMFormatFlagIsSignedInteger = kAudioFormatFlagIsSignedInteger,
    kLinearPCMFormatFlagIsPacked = kAudioFormatFlagIsPacked,
    kLinearPCMFormatFlagIsNonInterleaved = kAudioFormatFlagIsNonInterleaved,
#if TARGET_RT_BIG_ENDIAN
    kAudioFormatFlagsNativeEndian = kAudioFormatFlagIsBigEndian,
#else
    kAudioFormatFlagsNativeEndian = 0,
#endif
    kAudioFormatFlagsNativeFloatPacked = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked,
};

struct SMPTETime {
    SInt16 mSubframes;
    SInt16 mSubframeDivisor;
    UInt32 mCounter;
    UInt32 mType;
    UInt32 mFlags;
    SInt16 mHours;
    SInt16 mMinutes;
    SInt16 mSeconds;
    SInt16 mFrames;
};
typedef struct SMPTETime SMPTETime;

enum {
    kAudioTimeStampSampleTimeValid = (1U << 0),
    kAudioTimeStampHostTimeValid = (1U << 1),
    kAudioTimeStampRateScalarValid = (1U << 2),
    kAudioTimeStampWordClockTimeValid = (1U << 3),
    kAudioTimeStampSMPTETimeValid = (1U << 4),
    kAudioTimeStampSampleHostTimeValid = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid,
};

struct AudioTimeStamp {
    Float64 mSampleTime;
    UInt64 mHostTime;
    Float64 mRateScalar;
    UInt64 mWordClockTime;
    SMPTETime mSMPTETime;
    UInt32 mFlags;
    UInt32 mReserved;
};
typedef struct AudioTimeStamp AudioTimeStamp;

/* Channel labels, bits and layout tags. A tag's low 16 bits are its
   channel count. */
enum {
    kAudioChannelLabel_Unknown = 0xFFFFFFFF,
    kAudioChannelLabel_Unused = 0,
    kAudioChannelLabel_UseCoordinates = 100,
    kAudioChannelLabel_Left = 1,
    kAudioChannelLabel_Right = 2,
    kAudioChannelLabel_Center = 3,
    kAudioChannelLabel_LFEScreen = 4,
    kAudioChannelLabel_LeftSurround = 5,
    kAudioChannelLabel_RightSurround = 6,
    kAudioChannelLabel_LeftCenter = 7,
    kAudioChannelLabel_RightCenter = 8,
    kAudioChannelLabel_CenterSurround = 9,
    kAudioChannelLabel_LeftSurroundDirect = 10,
    kAudioChannelLabel_RightSurroundDirect = 11,
    kAudioChannelLabel_TopCenterSurround = 12,
    kAudioChannelLabel_VerticalHeightLeft = 13,
    kAudioChannelLabel_VerticalHeightCenter = 14,
    kAudioChannelLabel_VerticalHeightRight = 15,
    kAudioChannelLabel_TopBackLeft = 16,
    kAudioChannelLabel_TopBackCenter = 17,
    kAudioChannelLabel_TopBackRight = 18,
    kAudioChannelLabel_RearSurroundLeft = 33,
    kAudioChannelLabel_RearSurroundRight = 34,
    kAudioChannelLabel_LeftWide = 35,
    kAudioChannelLabel_RightWide = 36,
    kAudioChannelLabel_LFE2 = 37,
    kAudioChannelLabel_LeftTotal = 38,
    kAudioChannelLabel_RightTotal = 39,
    kAudioChannelLabel_HearingImpaired = 40,
    kAudioChannelLabel_Narration = 41,
    kAudioChannelLabel_Mono = 42,
    kAudioChannelLabel_DialogCentricMix = 43,
    kAudioChannelLabel_CenterSurroundDirect = 44,
    kAudioChannelLabel_Haptic = 45,
    kAudioChannelLabel_Headphones_Left = 301,
    kAudioChannelLabel_Headphones_Right = 302,
    kAudioChannelLabel_Discrete = 400,
    kAudioChannelLabel_Discrete_0 = (1U << 16) | 0,
};

typedef UInt32 AudioChannelBitmap;
enum {
    kAudioChannelBit_Left = (1U << 0),
    kAudioChannelBit_Right = (1U << 1),
    kAudioChannelBit_Center = (1U << 2),
    kAudioChannelBit_LFEScreen = (1U << 3),
    kAudioChannelBit_LeftSurround = (1U << 4),
    kAudioChannelBit_RightSurround = (1U << 5),
    kAudioChannelBit_LeftCenter = (1U << 6),
    kAudioChannelBit_RightCenter = (1U << 7),
    kAudioChannelBit_CenterSurround = (1U << 8),
    kAudioChannelBit_LeftSurroundDirect = (1U << 9),
    kAudioChannelBit_RightSurroundDirect = (1U << 10),
    kAudioChannelBit_TopCenterSurround = (1U << 11),
    kAudioChannelBit_VerticalHeightLeft = (1U << 12),
    kAudioChannelBit_VerticalHeightCenter = (1U << 13),
    kAudioChannelBit_VerticalHeightRight = (1U << 14),
    kAudioChannelBit_TopBackLeft = (1U << 15),
    kAudioChannelBit_TopBackCenter = (1U << 16),
    kAudioChannelBit_TopBackRight = (1U << 17),
};

typedef UInt32 AudioChannelFlags;
enum {
    kAudioChannelFlags_AllOff = 0,
    kAudioChannelFlags_RectangularCoordinates = (1U << 0),
    kAudioChannelFlags_SphericalCoordinates = (1U << 1),
    kAudioChannelFlags_Meters = (1U << 2),
};

#define AudioChannelLayoutTag_GetNumberOfChannels(layoutTag) ((UInt32)((layoutTag) & 0x0000FFFF))

enum {
    kAudioChannelLayoutTag_UseChannelDescriptions = (0U << 16) | 0,
    kAudioChannelLayoutTag_UseChannelBitmap = (1U << 16) | 0,
    kAudioChannelLayoutTag_Mono = (100U << 16) | 1,
    kAudioChannelLayoutTag_Stereo = (101U << 16) | 2,
    kAudioChannelLayoutTag_StereoHeadphones = (102U << 16) | 2,
    kAudioChannelLayoutTag_MatrixStereo = (103U << 16) | 2,
    kAudioChannelLayoutTag_MidSide = (104U << 16) | 2,
    kAudioChannelLayoutTag_XY = (105U << 16) | 2,
    kAudioChannelLayoutTag_Binaural = (106U << 16) | 2,
    kAudioChannelLayoutTag_Ambisonic_B_Format = (107U << 16) | 4,
    kAudioChannelLayoutTag_Quadraphonic = (108U << 16) | 4,
    kAudioChannelLayoutTag_Pentagonal = (109U << 16) | 5,
    kAudioChannelLayoutTag_Hexagonal = (110U << 16) | 6,
    kAudioChannelLayoutTag_Octagonal = (111U << 16) | 8,
    kAudioChannelLayoutTag_Cube = (112U << 16) | 8,
    kAudioChannelLayoutTag_MPEG_1_0 = kAudioChannelLayoutTag_Mono,
    kAudioChannelLayoutTag_MPEG_2_0 = kAudioChannelLayoutTag_Stereo,
    kAudioChannelLayoutTag_MPEG_3_0_A = (113U << 16) | 3,
    kAudioChannelLayoutTag_MPEG_3_0_B = (114U << 16) | 3,
    kAudioChannelLayoutTag_MPEG_4_0_A = (115U << 16) | 4,
    kAudioChannelLayoutTag_MPEG_4_0_B = (116U << 16) | 4,
    kAudioChannelLayoutTag_MPEG_5_0_A = (117U << 16) | 5,
    kAudioChannelLayoutTag_MPEG_5_0_B = (118U << 16) | 5,
    kAudioChannelLayoutTag_MPEG_5_0_C = (119U << 16) | 5,
    kAudioChannelLayoutTag_MPEG_5_0_D = (120U << 16) | 5,
    kAudioChannelLayoutTag_MPEG_5_1_A = (121U << 16) | 6,
    kAudioChannelLayoutTag_MPEG_5_1_B = (122U << 16) | 6,
    kAudioChannelLayoutTag_MPEG_5_1_C = (123U << 16) | 6,
    kAudioChannelLayoutTag_MPEG_5_1_D = (124U << 16) | 6,
    kAudioChannelLayoutTag_MPEG_6_1_A = (125U << 16) | 7,
    kAudioChannelLayoutTag_MPEG_7_1_A = (126U << 16) | 8,
    kAudioChannelLayoutTag_MPEG_7_1_B = (127U << 16) | 8,
    kAudioChannelLayoutTag_MPEG_7_1_C = (128U << 16) | 8,
    kAudioChannelLayoutTag_Emagic_Default_7_1 = (129U << 16) | 8,
    kAudioChannelLayoutTag_SMPTE_DTV = (130U << 16) | 8,
    kAudioChannelLayoutTag_ITU_1_0 = kAudioChannelLayoutTag_Mono,
    kAudioChannelLayoutTag_ITU_2_0 = kAudioChannelLayoutTag_Stereo,
    kAudioChannelLayoutTag_ITU_2_1 = (131U << 16) | 3,
    kAudioChannelLayoutTag_ITU_2_2 = (132U << 16) | 4,
    kAudioChannelLayoutTag_ITU_3_0 = kAudioChannelLayoutTag_MPEG_3_0_A,
    kAudioChannelLayoutTag_ITU_3_1 = kAudioChannelLayoutTag_MPEG_4_0_A,
    kAudioChannelLayoutTag_ITU_3_2 = kAudioChannelLayoutTag_MPEG_5_0_A,
    kAudioChannelLayoutTag_ITU_3_2_1 = kAudioChannelLayoutTag_MPEG_5_1_A,
    kAudioChannelLayoutTag_ITU_3_4_1 = kAudioChannelLayoutTag_MPEG_7_1_C,
    kAudioChannelLayoutTag_DVD_0 = kAudioChannelLayoutTag_Mono,
    kAudioChannelLayoutTag_DVD_1 = kAudioChannelLayoutTag_Stereo,
    kAudioChannelLayoutTag_AudioUnit_4 = kAudioChannelLayoutTag_Quadraphonic,
    kAudioChannelLayoutTag_AudioUnit_5 = kAudioChannelLayoutTag_Pentagonal,
    kAudioChannelLayoutTag_AudioUnit_6 = kAudioChannelLayoutTag_Hexagonal,
    kAudioChannelLayoutTag_AudioUnit_8 = kAudioChannelLayoutTag_Octagonal,
    kAudioChannelLayoutTag_AudioUnit_5_0 = kAudioChannelLayoutTag_MPEG_5_0_B,
    kAudioChannelLayoutTag_AudioUnit_6_0 = (139U << 16) | 6,
    kAudioChannelLayoutTag_AudioUnit_7_0 = (136U << 16) | 7,
    kAudioChannelLayoutTag_AudioUnit_7_0_Front = (148U << 16) | 7,
    kAudioChannelLayoutTag_AudioUnit_5_1 = kAudioChannelLayoutTag_MPEG_5_1_A,
    kAudioChannelLayoutTag_AudioUnit_6_1 = kAudioChannelLayoutTag_MPEG_6_1_A,
    kAudioChannelLayoutTag_AudioUnit_7_1 = kAudioChannelLayoutTag_MPEG_7_1_C,
    kAudioChannelLayoutTag_AudioUnit_7_1_Front = kAudioChannelLayoutTag_MPEG_7_1_A,
    kAudioChannelLayoutTag_AAC_3_0 = kAudioChannelLayoutTag_MPEG_3_0_B,
    kAudioChannelLayoutTag_AAC_Quadraphonic = kAudioChannelLayoutTag_Quadraphonic,
    kAudioChannelLayoutTag_AAC_4_0 = kAudioChannelLayoutTag_MPEG_4_0_B,
    kAudioChannelLayoutTag_AAC_5_0 = kAudioChannelLayoutTag_MPEG_5_0_D,
    kAudioChannelLayoutTag_AAC_5_1 = kAudioChannelLayoutTag_MPEG_5_1_D,
    kAudioChannelLayoutTag_AAC_6_0 = (141U << 16) | 6,
    kAudioChannelLayoutTag_AAC_6_1 = (142U << 16) | 7,
    kAudioChannelLayoutTag_AAC_7_0 = (143U << 16) | 7,
    kAudioChannelLayoutTag_AAC_7_1 = kAudioChannelLayoutTag_MPEG_7_1_B,
    kAudioChannelLayoutTag_AAC_7_1_B = (183U << 16) | 8,
    kAudioChannelLayoutTag_AAC_7_1_C = (184U << 16) | 8,
    kAudioChannelLayoutTag_AAC_Octagonal = (144U << 16) | 8,
    kAudioChannelLayoutTag_AC3_1_0_1 = (149U << 16) | 2,
    kAudioChannelLayoutTag_AC3_3_0 = (150U << 16) | 3,
    kAudioChannelLayoutTag_AC3_3_1 = (151U << 16) | 4,
    kAudioChannelLayoutTag_AC3_3_0_1 = (152U << 16) | 4,
    kAudioChannelLayoutTag_AC3_2_1_1 = (153U << 16) | 4,
    kAudioChannelLayoutTag_AC3_3_1_1 = (154U << 16) | 5,
    kAudioChannelLayoutTag_DiscreteInOrder = (147U << 16) | 0,
    kAudioChannelLayoutTag_WAVE_2_1 = (189U << 16) | 3,
    kAudioChannelLayoutTag_WAVE_3_0 = (190U << 16) | 3,
    kAudioChannelLayoutTag_WAVE_4_0_A = (191U << 16) | 4,
    kAudioChannelLayoutTag_WAVE_4_0_B = (192U << 16) | 4,
    kAudioChannelLayoutTag_WAVE_5_0_A = (193U << 16) | 5,
    kAudioChannelLayoutTag_WAVE_5_0_B = (194U << 16) | 5,
    kAudioChannelLayoutTag_WAVE_5_1_A = (195U << 16) | 6,
    kAudioChannelLayoutTag_WAVE_5_1_B = (196U << 16) | 6,
    kAudioChannelLayoutTag_WAVE_6_1 = (197U << 16) | 7,
    kAudioChannelLayoutTag_WAVE_7_1 = (198U << 16) | 8,
    kAudioChannelLayoutTag_Unknown = 0xFFFF0000,
};

struct AudioChannelDescription {
    AudioChannelLabel mChannelLabel;
    UInt32 mChannelFlags;
    Float32 mCoordinates[3];
};
typedef struct AudioChannelDescription AudioChannelDescription;

struct AudioChannelLayout {
    AudioChannelLayoutTag mChannelLayoutTag;
    UInt32 mChannelBitmap;
    UInt32 mNumberChannelDescriptions;
    AudioChannelDescription mChannelDescriptions[1];
};
typedef struct AudioChannelLayout AudioChannelLayout;

#ifdef __cplusplus
}
#endif

#endif
