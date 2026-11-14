/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * IOAudioFamily's types: stream formats, engine states and the status an
 * engine shares with its clients, control types. Minkar's own definitions
 * of the public interface.
 */

#ifndef _IOKIT_IOAUDIOTYPES_H
#define _IOKIT_IOAUDIOTYPES_H

#include <libkern/OSTypes.h>
#include <sys/cdefs.h>
#if !defined(KERNEL)
#include <mach/mach_types.h>
#endif

/* A sample rate: whole hertz and a fraction (of 2^32). */
typedef struct _IOAudioSampleRate {
    UInt32 whole;
    UInt32 fraction;
} IOAudioSampleRate;

/* How samples are laid out in a stream. */
typedef struct _IOAudioStreamFormat {
    UInt32 fNumChannels;
    UInt32 fSampleFormat;           /* kIOAudioStreamSampleFormat* */
    UInt32 fNumericRepresentation;  /* kIOAudioStreamNumericRepresentation* */
    UInt8  fBitDepth;               /* significant bits */
    UInt8  fBitWidth;               /* bits a sample takes */
    UInt8  fAlignment;              /* kIOAudioStreamAlignment* */
    UInt8  fByteOrder;              /* kIOAudioStreamByteOrder* */
    UInt8  fIsMixable;
    UInt32 fDriverTag;
} IOAudioStreamFormat;

typedef struct _IOAudioStreamFormatExtension {
    UInt32 fVersion;
    UInt32 fFlags;
    UInt32 fFramesPerPacket;
    UInt32 fBytesPerPacket;
} IOAudioStreamFormatExtension;

#define kFormatExtensionCurrentVersion  1

enum {
    kIOAudioStreamSampleFormatLinearPCM = 'lpcm',
    kIOAudioStreamSampleFormatIEEEFloat = 'ieee',
    kIOAudioStreamSampleFormatAC3       = 'ac-3',
    kIOAudioStreamSampleFormat1937AC3   = 'cac3'
};

enum {
    kIOAudioStreamNumericRepresentationSignedInt   = 'sint',
    kIOAudioStreamNumericRepresentationUnsignedInt = 'uint',
    kIOAudioStreamNumericRepresentationIEEE754Float = 'flot'
};

enum {
    kIOAudioStreamAlignmentLowByte  = 0,
    kIOAudioStreamAlignmentHighByte = 1
};

enum {
    kIOAudioStreamByteOrderBigEndian    = 0,
    kIOAudioStreamByteOrderLittleEndian = 1
};

typedef enum _IOAudioStreamDirection {
    kIOAudioStreamDirectionOutput = 0,
    kIOAudioStreamDirectionInput  = 1
} IOAudioStreamDirection;

typedef enum _IOAudioEngineState {
    kIOAudioEngineStopped = 0,
    kIOAudioEngineRunning = 1,
    kIOAudioEnginePaused  = 2,
    kIOAudioEngineResumed = 3
} IOAudioEngineState;

/*
 * What an engine shares with its clients, in a page they map: how many
 * times the sample buffer has wrapped, and when it last did (mach absolute
 * time). With the buffer's size and rate a client knows where the
 * hardware is between wraps.
 */
#define kIOAudioEngineCurrentStatusStructVersion 2

typedef struct _IOAudioEngineStatus {
    UInt32 fVersion;
    volatile UInt32 fCurrentLoopCount;
    volatile UInt64 fLastLoopTime;          /* AbsoluteTime */
    volatile UInt32 fEraseHeadSampleFrame;
    /* Minkar: odd while the engine writes the count and the time, even
       when they belong together; a reader reads it before and after them
       and tries again if it was odd or changed. */
    volatile UInt32 fSequence;
} IOAudioEngineStatus;

/* Terminal types (where a stream goes or comes from). */
enum {
    OUTPUT_UNDEFINED        = 0x0300,
    OUTPUT_SPEAKER          = 0x0301,
    OUTPUT_HEADPHONES       = 0x0302,
    INPUT_UNDEFINED         = 0x0200,
    INPUT_MICROPHONE        = 0x0201,
    EXTERNAL_LINE_CONNECTOR = 0x0603,
    EXTERNAL_DIGITAL_AUDIO_INTERFACE = 0x0602
};

/* Controls. */
enum {
    kIOAudioControlTypeLevel    = 'levl',
    kIOAudioControlTypeToggle   = 'togl',
    kIOAudioControlTypeSelector = 'slct'
};

enum {
    kIOAudioLevelControlSubTypeVolume = 'vlme',
    kIOAudioToggleControlSubTypeMute  = 'mute',
    kIOAudioSelectorControlSubTypeOutput = 'outp',
    kIOAudioSelectorControlSubTypeInput  = 'inpt'
};

enum {
    kIOAudioControlUsageOutput      = 'outp',
    kIOAudioControlUsageInput       = 'inpt',
    kIOAudioControlUsagePassThru    = 'thru'
};

#define kIOAudioControlChannelIDAll             0
#define kIOAudioControlChannelIDDefaultLeft     1
#define kIOAudioControlChannelIDDefaultRight    2
#define kIOAudioControlChannelNameAll           "All Channels"
#define kIOAudioControlChannelNameLeft          "Left"
#define kIOAudioControlChannelNameRight         "Right"

/* A level's range and its value in decibels: 16.16 fixed point. */
typedef SInt32 IOFixed16;

/*
 * Minkar: the engine user client's memory and methods (the audio
 * service's interface to an engine; the reference platform's is private).
 * clientMemoryForType: kIOAudioEngineMemoryStatus is the status page;
 * kIOAudioEngineMemoryStream + n is stream n's sample buffer.
 */
enum {
    kIOAudioEngineMemoryStatus = 0,
    kIOAudioEngineMemoryStream = 1
};

enum {
    kIOAudioEngineMethodStart = 0,      /* no arguments */
    kIOAudioEngineMethodStop = 1,       /* no arguments */
    kIOAudioEngineMethodGetInfo = 2,    /* out: IOAudioEngineInfo */
    kIOAudioEngineMethodGetStream = 3,  /* in: index; out: IOAudioEngineStreamInfo */
    kIOAudioEngineMethodCount = 4
};

typedef struct _IOAudioEngineInfo {
    UInt32 fState;                      /* IOAudioEngineState */
    UInt32 fNumStreams;
    UInt32 fNumSampleFramesPerBuffer;
    UInt32 fSampleRateWhole;
    UInt32 fSampleOffset;               /* frames kept ahead of the hardware */
    UInt32 fOutputLatency, fInputLatency;   /* frames */
} IOAudioEngineInfo;

typedef struct _IOAudioEngineStreamInfo {
    UInt32 fDirection;                  /* IOAudioStreamDirection */
    UInt32 fStartingChannelID;
    UInt32 fBufferSize;                 /* bytes */
    UInt32 fTerminalType;
    IOAudioStreamFormat fFormat;
} IOAudioEngineStreamInfo;

#endif /* _IOKIT_IOAUDIOTYPES_H */
