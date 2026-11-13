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
    kAudioFormatMPEG4AAC = 'aac ',
    kAudioFormatMPEGLayer3 = '.mp3',
    kAudioFormatOpus = 'opus',
    kAudioFormatFLAC = 'flac',
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
