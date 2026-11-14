/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Extended audio files: an AudioFile read or written in the client's
 * format through an AudioConverter (made when the client's format differs
 * from the file's). Positions are in the file's frames.
 */

#include "AUInternal.h"
#include <AudioToolbox/ExtendedAudioFile.h>
#include <stdlib.h>
#include <string.h>

#define kExtMagic       0x45784146u    /* 'ExAF' */
#define kIOFrames       4096
#define kNoMoreInput    'nomo'          /* the write's input is used up */

struct OpaqueExtAudioFile {
    uint32_t magic;
    AudioFileID file;
    Boolean ownsFile, writing;
    AudioStreamBasicDescription fileFormat, clientFormat;
    AudioConverterRef converter;
    void *io;                   /* kIOFrames of the file's format */
    SInt64 filePos;             /* next packet to read from the file */
    SInt64 writePos;            /* next packet to write */
    Float64 clientFrames;       /* frames given to the client since the last seek */
    SInt64 seekBase;            /* the file frame of the last seek */

    /* A write's input, for the converter. */
    const AudioBufferList *pendingInput;
    UInt32 pendingFrames;
};

static Boolean alive(ExtAudioFileRef e) {
    return e != NULL && e->magic == kExtMagic;
}

static Boolean sameFormat(const AudioStreamBasicDescription *a, const AudioStreamBasicDescription *b) {
    return a->mSampleRate == b->mSampleRate && a->mFormatID == b->mFormatID &&
           (a->mFormatFlags & ~kAudioFormatFlagIsPacked) == (b->mFormatFlags & ~kAudioFormatFlagIsPacked) &&
           a->mBytesPerFrame == b->mBytesPerFrame && a->mChannelsPerFrame == b->mChannelsPerFrame &&
           a->mBitsPerChannel == b->mBitsPerChannel;
}

static OSStatus wrap(AudioFileID file, Boolean writing, Boolean owns, ExtAudioFileRef *out) {
    ExtAudioFileRef e;
    UInt32 size = sizeof(AudioStreamBasicDescription);
    OSStatus status;

    e = calloc(1, sizeof(*e));
    if (e == NULL)
        return kAudio_MemFullError;
    status = AudioFileGetProperty(file, kAudioFilePropertyDataFormat, &size, &e->fileFormat);
    if (status != noErr) {
        free(e);
        return status;
    }
    e->io = malloc((size_t)kIOFrames * e->fileFormat.mBytesPerFrame);
    if (e->io == NULL) {
        free(e);
        return kAudio_MemFullError;
    }
    e->magic = kExtMagic;
    e->file = file;
    e->ownsFile = owns;
    e->writing = writing;
    e->clientFormat = e->fileFormat;
    if (writing) {
        UInt64 packets = 0;
        size = sizeof(packets);
        AudioFileGetProperty(file, kAudioFilePropertyAudioDataPacketCount, &size, &packets);
        e->writePos = (SInt64)packets;
    }
    *out = e;
    return noErr;
}

OSStatus ExtAudioFileWrapAudioFileID(AudioFileID file, Boolean forWriting, ExtAudioFileRef *out) {
    if (file == NULL || out == NULL)
        return kAudio_ParamError;
    *out = NULL;
    return wrap(file, forWriting, false, out);
}

OSStatus ExtAudioFileOpenURL(CFURLRef url, ExtAudioFileRef *out) {
    AudioFileID file;
    OSStatus status;

    if (out == NULL)
        return kAudio_ParamError;
    *out = NULL;
    status = AudioFileOpenURL(url, kAudioFileReadPermission, 0, &file);
    if (status != noErr)
        return status;
    status = wrap(file, false, true, out);
    if (status != noErr)
        AudioFileClose(file);
    return status;
}

OSStatus ExtAudioFileCreateWithURL(CFURLRef url, AudioFileTypeID type, const AudioStreamBasicDescription *format,
                                   const AudioChannelLayout *layout, UInt32 flags, ExtAudioFileRef *out) {
    AudioFileID file;
    OSStatus status;

    if (out == NULL || format == NULL)
        return kAudio_ParamError;
    *out = NULL;
    status = AudioFileCreateWithURL(url, type, format, flags, &file);
    if (status != noErr)
        return status;
    status = wrap(file, true, true, out);
    if (status != noErr)
        AudioFileClose(file);
    return status;
}

static OSStatus writeInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                           AudioStreamPacketDescription **descriptions, void *userData);

/* At the end of writing, what the converter still holds (a resampler's
   last frames). */
static OSStatus drain(ExtAudioFileRef e) {
    OSStatus status = noErr;

    if (!e->writing || e->converter == NULL)
        return noErr;
    e->pendingInput = NULL;
    e->pendingFrames = 0;
    for (int guard = 0; guard < 64; guard++) {
        AudioBufferList out;
        UInt32 frames = kIOFrames;
        out.mNumberBuffers = 1;
        out.mBuffers[0].mNumberChannels = e->fileFormat.mChannelsPerFrame;
        out.mBuffers[0].mDataByteSize = kIOFrames * e->fileFormat.mBytesPerFrame;
        out.mBuffers[0].mData = e->io;
        status = AudioConverterFillComplexBuffer(e->converter, writeInput, e, &frames, &out, NULL);
        if (frames == 0)
            break;
        status = AudioFileWritePackets(e->file, false, frames * e->fileFormat.mBytesPerFrame, NULL, e->writePos,
                                       &frames, e->io);
        if (status != noErr)
            break;
        e->writePos += frames;
    }
    return status == kNoMoreInput ? noErr : status;
}

OSStatus ExtAudioFileDispose(ExtAudioFileRef e) {
    OSStatus status;

    if (!alive(e))
        return kAudio_ParamError;
    status = drain(e);
    if (e->converter != NULL)
        AudioConverterDispose(e->converter);
    if (e->ownsFile) {
        OSStatus s = AudioFileClose(e->file);
        if (status == noErr)
            status = s;
    }
    free(e->io);
    e->magic = 0;
    free(e);
    return status;
}

/* ---- Reading ---------------------------------------------------------------- */

static OSStatus readInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                          AudioStreamPacketDescription **descriptions, void *userData) {
    ExtAudioFileRef e = userData;
    UInt32 n = *ioPackets < kIOFrames ? *ioPackets : kIOFrames;
    UInt32 bytes = n * e->fileFormat.mBytesPerFrame;
    OSStatus status = AudioFileReadPacketData(e->file, false, &bytes, NULL, e->filePos, &n, e->io);

    if (status != noErr) {
        *ioPackets = 0;
        return status;
    }
    e->filePos += n;
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = e->fileFormat.mChannelsPerFrame;
    data->mBuffers[0].mDataByteSize = bytes;
    data->mBuffers[0].mData = e->io;
    *ioPackets = n;     /* 0 at the end of the file */
    return noErr;
}

OSStatus ExtAudioFileRead(ExtAudioFileRef e, UInt32 *ioFrames, AudioBufferList *data) {
    OSStatus status;

    if (!alive(e) || ioFrames == NULL || data == NULL)
        return kAudio_ParamError;
    if (e->writing)
        return kExtAudioFileError_InvalidOperationOrder;
    if (e->converter == NULL) {
        /* The client's format is the file's: straight into its buffer. */
        UInt32 n = *ioFrames, bytes = data->mBuffers[0].mDataByteSize;
        status = AudioFileReadPacketData(e->file, false, &bytes, NULL, e->filePos, &n, data->mBuffers[0].mData);
        if (status != noErr) {
            *ioFrames = 0;
            return status;
        }
        e->filePos += n;
        e->clientFrames += n;
        data->mBuffers[0].mDataByteSize = bytes;
        *ioFrames = n;
        return noErr;
    }
    status = AudioConverterFillComplexBuffer(e->converter, readInput, e, ioFrames, data, NULL);
    e->clientFrames += *ioFrames;
    return status;
}

/* ---- Writing ---------------------------------------------------------------- */

static OSStatus writeInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                           AudioStreamPacketDescription **descriptions, void *userData) {
    ExtAudioFileRef e = userData;

    if (e->pendingInput == NULL) {
        *ioPackets = 0;
        /* 0 frames and no error is the end (when draining). */
        return e->pendingFrames == 0 ? noErr : kNoMoreInput;
    }
    /* The whole write at once; the converter keeps what it does not use. */
    data->mNumberBuffers = e->pendingInput->mNumberBuffers;
    for (UInt32 b = 0; b < data->mNumberBuffers; b++)
        data->mBuffers[b] = e->pendingInput->mBuffers[b];
    *ioPackets = e->pendingFrames;
    e->pendingInput = NULL;
    e->pendingFrames = 1;   /* marks "used up" rather than "ended" */
    return noErr;
}

OSStatus ExtAudioFileWrite(ExtAudioFileRef e, UInt32 frames, const AudioBufferList *data) {
    OSStatus status = noErr;

    if (!alive(e) || (data == NULL && frames > 0))
        return kAudio_ParamError;
    if (!e->writing)
        return kExtAudioFileError_InvalidOperationOrder;
    if (frames == 0)
        return noErr;
    if (e->converter == NULL) {
        UInt32 n = frames;
        status = AudioFileWritePackets(e->file, false, data->mBuffers[0].mDataByteSize, NULL, e->writePos, &n,
                                       data->mBuffers[0].mData);
        e->writePos += n;
        return status;
    }
    e->pendingInput = data;
    e->pendingFrames = frames;
    for (int guard = 0; guard < 100000; guard++) {
        AudioBufferList out;
        UInt32 n = kIOFrames;
        out.mNumberBuffers = 1;
        out.mBuffers[0].mNumberChannels = e->fileFormat.mChannelsPerFrame;
        out.mBuffers[0].mDataByteSize = kIOFrames * e->fileFormat.mBytesPerFrame;
        out.mBuffers[0].mData = e->io;
        status = AudioConverterFillComplexBuffer(e->converter, writeInput, e, &n, &out, NULL);
        if (n > 0) {
            OSStatus s = AudioFileWritePackets(e->file, false, n * e->fileFormat.mBytesPerFrame, NULL,
                                               e->writePos, &n, e->io);
            if (s != noErr)
                return s;
            e->writePos += n;
        }
        if (status != noErr || n == 0)
            break;
    }
    e->pendingInput = NULL;
    e->pendingFrames = 0;
    return status == kNoMoreInput ? noErr : status;
}

OSStatus ExtAudioFileWriteAsync(ExtAudioFileRef e, UInt32 frames, const AudioBufferList *data) {
    /* A call to prime the async path has no data. */
    if (alive(e) && data == NULL && frames == 0)
        return noErr;
    return ExtAudioFileWrite(e, frames, data);
}

/* ---- Position -------------------------------------------------------------- */

OSStatus ExtAudioFileSeek(ExtAudioFileRef e, SInt64 frame) {
    UInt64 packets = 0;
    UInt32 size = sizeof(packets);

    if (!alive(e))
        return kAudio_ParamError;
    if (e->writing)
        return kExtAudioFileError_InvalidSeek;
    AudioFileGetProperty(e->file, kAudioFilePropertyAudioDataPacketCount, &size, &packets);
    if (frame < 0 || frame > (SInt64)packets)
        return kExtAudioFileError_InvalidSeek;
    e->filePos = frame;
    e->seekBase = frame;
    e->clientFrames = 0;
    if (e->converter != NULL)
        AudioConverterReset(e->converter);
    return noErr;
}

OSStatus ExtAudioFileTell(ExtAudioFileRef e, SInt64 *out) {
    if (!alive(e) || out == NULL)
        return kAudio_ParamError;
    if (e->writing)
        *out = e->writePos;
    else
        *out = e->seekBase + (SInt64)(e->clientFrames * e->fileFormat.mSampleRate / e->clientFormat.mSampleRate + 0.5);
    return noErr;
}

/* ---- Properties ------------------------------------------------------------ */

static UInt32 propertySize(ExtAudioFileRef e, ExtAudioFilePropertyID id, Boolean *writable) {
    *writable = false;
    switch (id) {
    case kExtAudioFileProperty_FileDataFormat:
        return sizeof(AudioStreamBasicDescription);
    case kExtAudioFileProperty_ClientDataFormat:
        *writable = true;
        return sizeof(AudioStreamBasicDescription);
    case kExtAudioFileProperty_AudioConverter:
        return sizeof(AudioConverterRef);
    case kExtAudioFileProperty_AudioFile:
        return sizeof(AudioFileID);
    case kExtAudioFileProperty_FileMaxPacketSize:
    case kExtAudioFileProperty_ClientMaxPacketSize:
    case kExtAudioFileProperty_IOBufferSizeBytes:
        return sizeof(UInt32);
    case kExtAudioFileProperty_CodecManufacturer:
        *writable = true;
        return sizeof(UInt32);
    case kExtAudioFileProperty_FileLengthFrames:
        *writable = e->writing;
        return sizeof(SInt64);
    case kExtAudioFileProperty_ConverterConfig:
        *writable = true;
        return sizeof(CFPropertyListRef);
    default:
        return 0;
    }
}

OSStatus ExtAudioFileGetPropertyInfo(ExtAudioFileRef e, ExtAudioFilePropertyID id, UInt32 *outSize,
                                     Boolean *outWritable) {
    Boolean writable;
    UInt32 size;

    if (!alive(e))
        return kAudio_ParamError;
    size = propertySize(e, id, &writable);
    if (size == 0)
        return kExtAudioFileError_InvalidProperty;
    if (outSize != NULL)
        *outSize = size;
    if (outWritable != NULL)
        *outWritable = writable;
    return noErr;
}

OSStatus ExtAudioFileGetProperty(ExtAudioFileRef e, ExtAudioFilePropertyID id, UInt32 *ioSize, void *data) {
    Boolean writable;
    UInt32 size;

    if (!alive(e) || ioSize == NULL || data == NULL)
        return kAudio_ParamError;
    size = propertySize(e, id, &writable);
    if (size == 0)
        return kExtAudioFileError_InvalidProperty;
    if (*ioSize < size)
        return kExtAudioFileError_InvalidPropertySize;
    switch (id) {
    case kExtAudioFileProperty_FileDataFormat:
        *(AudioStreamBasicDescription *)data = e->fileFormat;
        break;
    case kExtAudioFileProperty_ClientDataFormat:
        *(AudioStreamBasicDescription *)data = e->clientFormat;
        break;
    case kExtAudioFileProperty_AudioConverter:
        *(AudioConverterRef *)data = e->converter;
        break;
    case kExtAudioFileProperty_AudioFile:
        *(AudioFileID *)data = e->file;
        break;
    case kExtAudioFileProperty_FileMaxPacketSize:
        *(UInt32 *)data = e->fileFormat.mBytesPerPacket;
        break;
    case kExtAudioFileProperty_ClientMaxPacketSize:
        *(UInt32 *)data = e->clientFormat.mBytesPerPacket;
        break;
    case kExtAudioFileProperty_IOBufferSizeBytes:
        *(UInt32 *)data = kIOFrames * e->fileFormat.mBytesPerFrame;
        break;
    case kExtAudioFileProperty_CodecManufacturer:
        *(UInt32 *)data = 0;
        break;
    case kExtAudioFileProperty_FileLengthFrames: {
        UInt64 packets = 0;
        UInt32 s = sizeof(packets);
        AudioFileGetProperty(e->file, kAudioFilePropertyAudioDataPacketCount, &s, &packets);
        *(SInt64 *)data = e->writing && e->writePos > (SInt64)packets ? e->writePos : (SInt64)packets;
        break;
    }
    case kExtAudioFileProperty_ConverterConfig:
        *(CFPropertyListRef *)data = NULL;
        break;
    }
    *ioSize = size;
    return noErr;
}

OSStatus ExtAudioFileSetProperty(ExtAudioFileRef e, ExtAudioFilePropertyID id, UInt32 size, const void *data) {
    if (!alive(e) || data == NULL)
        return kAudio_ParamError;
    switch (id) {
    case kExtAudioFileProperty_ClientDataFormat: {
        const AudioStreamBasicDescription *f = data;
        AudioConverterRef c = NULL;
        OSStatus status;
        if (size < sizeof(*f))
            return kExtAudioFileError_InvalidPropertySize;
        if (f->mFormatID != kAudioFormatLinearPCM)
            return kExtAudioFileError_NonPCMClientFormat;
        if (!_AUFormatIsSupported(f))
            return kExtAudioFileError_InvalidDataFormat;
        if (!sameFormat(f, &e->fileFormat)) {
            status = e->writing ? AudioConverterNew(f, &e->fileFormat, &c)
                                : AudioConverterNew(&e->fileFormat, f, &c);
            if (status != noErr)
                return kExtAudioFileError_InvalidDataFormat;
        }
        if (e->converter != NULL) {
            drain(e);
            AudioConverterDispose(e->converter);
        }
        e->converter = c;
        e->clientFormat = *f;
        /* Reading goes on from the same place in the file. */
        e->seekBase = e->filePos;
        e->clientFrames = 0;
        return noErr;
    }
    case kExtAudioFileProperty_CodecManufacturer:
    case kExtAudioFileProperty_ConverterConfig:
        return noErr;
    case kExtAudioFileProperty_FileLengthFrames:
        if (size < sizeof(SInt64))
            return kExtAudioFileError_InvalidPropertySize;
        /* Shortening a file is not there yet: only its length as written. */
        return e->writing && *(const SInt64 *)data == e->writePos ? noErr : kExtAudioFileError_InvalidOperationOrder;
    default:
        return propertySize(e, id, &(Boolean){ false }) != 0 ? kExtAudioFileError_InvalidOperationOrder
                                                             : kExtAudioFileError_InvalidProperty;
    }
}
