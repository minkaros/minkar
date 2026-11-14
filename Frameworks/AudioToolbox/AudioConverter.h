/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio converters: from one stream format to another (sample format,
 * channels and rate).
 */

#ifndef __AUDIOCONVERTER__
#define __AUDIOCONVERTER__

#include <CoreAudio/CoreAudioTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueAudioConverter *AudioConverterRef;
typedef UInt32 AudioConverterPropertyID;

enum {
    kAudioConverterPropertyMinimumInputBufferSize = 'mibs',
    kAudioConverterPropertyMinimumOutputBufferSize = 'mobs',
    kAudioConverterPropertyMaximumInputBufferSize = 'xibs',
    kAudioConverterPropertyMaximumInputPacketSize = 'xips',
    kAudioConverterPropertyMaximumOutputPacketSize = 'xops',
    kAudioConverterPropertyCalculateInputBufferSize = 'cibs',
    kAudioConverterPropertyCalculateOutputBufferSize = 'cobs',
    kAudioConverterPropertyInputCodecParameters = 'icdp',
    kAudioConverterPropertyOutputCodecParameters = 'ocdp',
    kAudioConverterSampleRateConverterAlgorithm = 'srci',
    kAudioConverterSampleRateConverterComplexity = 'srca',
    kAudioConverterSampleRateConverterQuality = 'srcq',
    kAudioConverterSampleRateConverterInitialPhase = 'srcp',
    kAudioConverterCodecQuality = 'cdqu',
    kAudioConverterPrimeMethod = 'prmm',
    kAudioConverterPrimeInfo = 'prim',
    kAudioConverterChannelMap = 'chmp',
    kAudioConverterDecompressionMagicCookie = 'dmgc',
    kAudioConverterCompressionMagicCookie = 'cmgc',
    kAudioConverterEncodeBitRate = 'brat',
    kAudioConverterEncodeAdjustableSampleRate = 'ajsr',
    kAudioConverterInputChannelLayout = 'icl ',
    kAudioConverterOutputChannelLayout = 'ocl ',
    kAudioConverterApplicableEncodeBitRates = 'aebr',
    kAudioConverterAvailableEncodeBitRates = 'vebr',
    kAudioConverterApplicableEncodeSampleRates = 'aesr',
    kAudioConverterAvailableEncodeSampleRates = 'vesr',
    kAudioConverterAvailableEncodeChannelLayoutTags = 'aecl',
    kAudioConverterCurrentOutputStreamDescription = 'acod',
    kAudioConverterCurrentInputStreamDescription = 'acid',
    kAudioConverterPropertySettings = 'acps',
    kAudioConverterPropertyBitDepthHint = 'acbd',
    kAudioConverterPropertyFormatList = 'flst',
};

enum {
    kAudioConverterQuality_Max = 0x7F,
    kAudioConverterQuality_High = 0x60,
    kAudioConverterQuality_Medium = 0x40,
    kAudioConverterQuality_Low = 0x20,
    kAudioConverterQuality_Min = 0,
};

enum {
    kAudioConverterSampleRateConverterComplexity_Linear = 'line',
    kAudioConverterSampleRateConverterComplexity_Normal = 'norm',
    kAudioConverterSampleRateConverterComplexity_Mastering = 'bats',
    kAudioConverterSampleRateConverterComplexity_MinimumPhase = 'minp',
};

enum {
    kConverterPrimeMethod_Pre = 0,
    kConverterPrimeMethod_Normal = 1,
    kConverterPrimeMethod_None = 2,
};

struct AudioConverterPrimeInfo {
    UInt32 leadingFrames;
    UInt32 trailingFrames;
};
typedef struct AudioConverterPrimeInfo AudioConverterPrimeInfo;

enum {
    kAudioConverterErr_FormatNotSupported = 'fmt?',
    kAudioConverterErr_OperationNotSupported = 0x6F703F3F,  /* 'op??' */
    kAudioConverterErr_PropertyNotSupported = 'prop',
    kAudioConverterErr_InvalidInputSize = 'insz',
    kAudioConverterErr_InvalidOutputSize = 'otsz',
    kAudioConverterErr_UnspecifiedError = 'what',
    kAudioConverterErr_BadPropertySizeError = '!siz',
    kAudioConverterErr_RequiresPacketDescriptionsError = '!pkd',
    kAudioConverterErr_InputSampleRateOutOfRange = '!isr',
    kAudioConverterErr_OutputSampleRateOutOfRange = '!osr',
};

typedef OSStatus (*AudioConverterInputDataProc)(AudioConverterRef inAudioConverter, UInt32 *ioDataSize,
    void **outData, void *inUserData);

typedef OSStatus (*AudioConverterComplexInputDataProc)(AudioConverterRef inAudioConverter,
    UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
    AudioStreamPacketDescription **outDataPacketDescription, void *inUserData);

OSStatus AudioConverterNew(const AudioStreamBasicDescription *inSourceFormat,
    const AudioStreamBasicDescription *inDestinationFormat, AudioConverterRef *outAudioConverter);
OSStatus AudioConverterNewSpecific(const AudioStreamBasicDescription *inSourceFormat,
    const AudioStreamBasicDescription *inDestinationFormat, UInt32 inNumberClassDescriptions,
    const void *inClassDescriptions, AudioConverterRef *outAudioConverter);
OSStatus AudioConverterDispose(AudioConverterRef inAudioConverter);
OSStatus AudioConverterReset(AudioConverterRef inAudioConverter);
OSStatus AudioConverterGetPropertyInfo(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID,
    UInt32 *outSize, Boolean *outWritable);
OSStatus AudioConverterGetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID,
    UInt32 *ioPropertyDataSize, void *outPropertyData);
OSStatus AudioConverterSetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID,
    UInt32 inPropertyDataSize, const void *inPropertyData);
OSStatus AudioConverterConvertBuffer(AudioConverterRef inAudioConverter, UInt32 inInputDataSize,
    const void *inInputData, UInt32 *ioOutputDataSize, void *outOutputData);
OSStatus AudioConverterFillComplexBuffer(AudioConverterRef inAudioConverter,
    AudioConverterComplexInputDataProc inInputDataProc, void *inInputDataProcUserData,
    UInt32 *ioOutputDataPacketSize, AudioBufferList *outOutputData,
    AudioStreamPacketDescription *outPacketDescription);
OSStatus AudioConverterConvertComplexBuffer(AudioConverterRef inAudioConverter, UInt32 inNumberPCMFrames,
    const AudioBufferList *inInputData, AudioBufferList *outOutputData);
OSStatus AudioConverterFillBuffer(AudioConverterRef inAudioConverter, AudioConverterInputDataProc inInputDataProc,
    void *inInputDataProcUserData, UInt32 *ioOutputDataSize, void *outOutputData);

#ifdef __cplusplus
}
#endif

#endif
