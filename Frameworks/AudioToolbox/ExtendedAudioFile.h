/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Extended audio files: an audio file read and written in the app's own
 * format, converted on the way.
 */

#ifndef __EXTENDEDAUDIOFILE__
#define __EXTENDEDAUDIOFILE__

#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueExtAudioFile *ExtAudioFileRef;
typedef UInt32 ExtAudioFilePropertyID;

enum {
    kExtAudioFileProperty_FileDataFormat = 'ffmt',
    kExtAudioFileProperty_FileChannelLayout = 'fclo',
    kExtAudioFileProperty_ClientDataFormat = 'cfmt',
    kExtAudioFileProperty_ClientChannelLayout = 'cclo',
    kExtAudioFileProperty_CodecManufacturer = 'cman',
    kExtAudioFileProperty_AudioConverter = 'acnv',
    kExtAudioFileProperty_AudioFile = 'afil',
    kExtAudioFileProperty_FileMaxPacketSize = 'fmps',
    kExtAudioFileProperty_ClientMaxPacketSize = 'cmps',
    kExtAudioFileProperty_FileLengthFrames = '#frm',
    kExtAudioFileProperty_ConverterConfig = 'accf',
    kExtAudioFileProperty_IOBufferSizeBytes = 'iobs',
    kExtAudioFileProperty_IOBuffer = 'iobf',
    kExtAudioFileProperty_PacketTable = 'xpti',
};

enum {
    kExtAudioFileError_InvalidProperty = -66561,
    kExtAudioFileError_InvalidPropertySize = -66562,
    kExtAudioFileError_NonPCMClientFormat = -66563,
    kExtAudioFileError_InvalidChannelMap = -66564,
    kExtAudioFileError_InvalidOperationOrder = -66565,
    kExtAudioFileError_InvalidDataFormat = -66566,
    kExtAudioFileError_MaxPacketSizeUnknown = -66567,
    kExtAudioFileError_InvalidSeek = -66568,
    kExtAudioFileError_AsyncWriteTooLarge = -66569,
    kExtAudioFileError_AsyncWriteBufferOverflow = -66570,
};

OSStatus ExtAudioFileOpenURL(CFURLRef inURL, ExtAudioFileRef *outExtAudioFile);
OSStatus ExtAudioFileWrapAudioFileID(AudioFileID inFileID, Boolean inForWriting, ExtAudioFileRef *outExtAudioFile);
OSStatus ExtAudioFileCreateWithURL(CFURLRef inURL, AudioFileTypeID inFileType,
    const AudioStreamBasicDescription *inStreamDesc, const AudioChannelLayout *inChannelLayout, UInt32 inFlags,
    ExtAudioFileRef *outExtAudioFile);
OSStatus ExtAudioFileDispose(ExtAudioFileRef inExtAudioFile);
OSStatus ExtAudioFileRead(ExtAudioFileRef inExtAudioFile, UInt32 *ioNumberFrames, AudioBufferList *ioData);
OSStatus ExtAudioFileWrite(ExtAudioFileRef inExtAudioFile, UInt32 inNumberFrames, const AudioBufferList *ioData);
OSStatus ExtAudioFileWriteAsync(ExtAudioFileRef inExtAudioFile, UInt32 inNumberFrames, const AudioBufferList *ioData);
OSStatus ExtAudioFileSeek(ExtAudioFileRef inExtAudioFile, SInt64 inFrameOffset);
OSStatus ExtAudioFileTell(ExtAudioFileRef inExtAudioFile, SInt64 *outFrameOffset);
OSStatus ExtAudioFileGetPropertyInfo(ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID,
    UInt32 *outSize, Boolean *outWritable);
OSStatus ExtAudioFileGetProperty(ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID,
    UInt32 *ioPropertyDataSize, void *outPropertyData);
OSStatus ExtAudioFileSetProperty(ExtAudioFileRef inExtAudioFile, ExtAudioFilePropertyID inPropertyID,
    UInt32 inPropertyDataSize, const void *inPropertyData);

#ifdef __cplusplus
}
#endif

#endif
