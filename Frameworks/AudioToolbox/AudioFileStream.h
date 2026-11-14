/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio file streams: a sound file's bytes, given as they arrive (from a
 * network, say), parsed into its properties and its packets.
 */

#ifndef __AUDIOFILESTREAM__
#define __AUDIOFILESTREAM__

#include <AudioToolbox/AudioFile.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueAudioFileStreamID *AudioFileStreamID;
typedef UInt32 AudioFileStreamPropertyID;

typedef UInt32 AudioFileStreamPropertyFlags;
enum {
    kAudioFileStreamPropertyFlag_PropertyIsCached = 1,
    kAudioFileStreamPropertyFlag_CacheProperty = 2,
};

typedef UInt32 AudioFileStreamParseFlags;
enum {
    kAudioFileStreamParseFlag_Discontinuity = 1,
};

typedef UInt32 AudioFileStreamSeekFlags;
enum {
    kAudioFileStreamSeekFlag_OffsetIsEstimated = 1,
};

enum {
    kAudioFileStreamError_UnsupportedFileType = 'typ?',
    kAudioFileStreamError_UnsupportedDataFormat = 'fmt?',
    kAudioFileStreamError_UnsupportedProperty = 'pty?',
    kAudioFileStreamError_BadPropertySize = '!siz',
    kAudioFileStreamError_NotOptimized = 'optm',
    kAudioFileStreamError_InvalidPacketOffset = 'pck?',
    kAudioFileStreamError_InvalidFile = 'dta?',
    kAudioFileStreamError_ValueUnknown = 'unk?',
    kAudioFileStreamError_DataUnavailable = 'more',
    kAudioFileStreamError_IllegalOperation = 'nope',
    kAudioFileStreamError_UnspecifiedError = 'wht?',
    kAudioFileStreamError_DiscontinuityCantRecover = 'dsc!',
};

enum {
    kAudioFileStreamProperty_ReadyToProducePackets = 'redy',
    kAudioFileStreamProperty_FileFormat = 'ffmt',
    kAudioFileStreamProperty_DataFormat = 'dfmt',
    kAudioFileStreamProperty_FormatList = 'flst',
    kAudioFileStreamProperty_MagicCookieData = 'mgic',
    kAudioFileStreamProperty_AudioDataByteCount = 'bcnt',
    kAudioFileStreamProperty_AudioDataPacketCount = 'pcnt',
    kAudioFileStreamProperty_MaximumPacketSize = 'psze',
    kAudioFileStreamProperty_DataOffset = 'doff',
    kAudioFileStreamProperty_ChannelLayout = 'cmap',
    kAudioFileStreamProperty_PacketToFrame = 'pkfr',
    kAudioFileStreamProperty_FrameToPacket = 'frpk',
    kAudioFileStreamProperty_RestrictsRandomAccess = 'rrap',
    kAudioFileStreamProperty_PacketToRollDistance = 'pkrl',
    kAudioFileStreamProperty_PreviousIndependentPacket = 'pind',
    kAudioFileStreamProperty_NextIndependentPacket = 'nind',
    kAudioFileStreamProperty_PacketToDependencyInfo = 'pkdp',
    kAudioFileStreamProperty_PacketToByte = 'pkby',
    kAudioFileStreamProperty_ByteToPacket = 'bypk',
    kAudioFileStreamProperty_PacketTableInfo = 'pnfo',
    kAudioFileStreamProperty_PacketSizeUpperBound = 'pkub',
    kAudioFileStreamProperty_AverageBytesPerPacket = 'abpp',
    kAudioFileStreamProperty_BitRate = 'brat',
    kAudioFileStreamProperty_InfoDictionary = 'info',
};

typedef void (*AudioFileStream_PropertyListenerProc)(void *inClientData, AudioFileStreamID inAudioFileStream,
    AudioFileStreamPropertyID inPropertyID, AudioFileStreamPropertyFlags *ioFlags);
typedef void (*AudioFileStream_PacketsProc)(void *inClientData, UInt32 inNumberBytes, UInt32 inNumberPackets,
    const void *inInputData, AudioStreamPacketDescription *inPacketDescriptions);

OSStatus AudioFileStreamOpen(void *inClientData, AudioFileStream_PropertyListenerProc inPropertyListenerProc,
    AudioFileStream_PacketsProc inPacketsProc, AudioFileTypeID inFileTypeHint,
    AudioFileStreamID *outAudioFileStream);
OSStatus AudioFileStreamParseBytes(AudioFileStreamID inAudioFileStream, UInt32 inDataByteSize,
    const void *inData, AudioFileStreamParseFlags inFlags);
OSStatus AudioFileStreamSeek(AudioFileStreamID inAudioFileStream, SInt64 inPacketOffset,
    SInt64 *outDataByteOffset, AudioFileStreamSeekFlags *ioFlags);
OSStatus AudioFileStreamGetPropertyInfo(AudioFileStreamID inAudioFileStream, AudioFileStreamPropertyID inPropertyID,
    UInt32 *outPropertyDataSize, Boolean *outWritable);
OSStatus AudioFileStreamGetProperty(AudioFileStreamID inAudioFileStream, AudioFileStreamPropertyID inPropertyID,
    UInt32 *ioPropertyDataSize, void *outPropertyData);
OSStatus AudioFileStreamSetProperty(AudioFileStreamID inAudioFileStream, AudioFileStreamPropertyID inPropertyID,
    UInt32 inPropertyDataSize, const void *inPropertyData);
OSStatus AudioFileStreamClose(AudioFileStreamID inAudioFileStream);

#ifdef __cplusplus
}
#endif

#endif
