/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio file streams: bytes given as they arrive are kept until a whole
 * header or packet is there. Linear PCM in WAVE, AIFF, AIFC and CAF (the
 * header read as AudioFile reads it) gives packets of whole frames; AAC
 * in ADTS and MPEG audio (layers 1 to 3, after an ID3 tag if there is
 * one) give one packet per frame, with their descriptions; ADTS's packets
 * are the frames without their headers, and its magic cookie is an ES
 * descriptor holding the AudioSpecificConfig. The frame layouts are the
 * public standards'.
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioFileStream.h>
#include <AudioToolbox/AudioFormat.h>
#include <stdlib.h>
#include <string.h>

#define kStreamMagic    0x41465374u    /* 'AFSt' */
#define kMaxHeader      (1 << 20)      /* a PCM header longer than this is not one */
#define kBatch          64             /* packets handed over at a time */

typedef enum { kKindUnknown, kKindPCM, kKindADTS, kKindMPEG } Kind;

struct OpaqueAudioFileStreamID {
    uint32_t magic;
    void *client;
    AudioFileStream_PropertyListenerProc listener;
    AudioFileStream_PacketsProc packets;
    AudioFileTypeID hint;

    Kind kind;
    Boolean ready, failed;
    AudioFileTypeID type;
    AudioStreamBasicDescription format;
    SInt64 dataOffset;          /* where the audio data starts in the stream */
    SInt64 dataBytes;           /* its length, or -1 */
    SInt64 dataSeen;            /* audio bytes handed over */
    UInt32 skip;                /* bytes still to drop (an ID3 tag) */

    unsigned char *pending;     /* bytes not yet used */
    UInt32 pendingSize, pendingCapacity;
    SInt64 position;            /* the stream offset of pending's first byte */

    unsigned char cookie[64];
    UInt32 cookieSize;
    SInt64 packetCount;
    UInt64 packetBytes;
    UInt32 maxPacket;
    UInt32 bitRate;

    unsigned char *batch;       /* packets gathered for one call */
    UInt32 batchSize, batchCapacity;
    AudioStreamPacketDescription descriptions[kBatch];
    UInt32 descriptionCount;
};

static Boolean alive(AudioFileStreamID s) {
    return s != NULL && s->magic == kStreamMagic;
}

OSStatus AudioFileStreamOpen(void *client, AudioFileStream_PropertyListenerProc listener,
                             AudioFileStream_PacketsProc packets, AudioFileTypeID hint, AudioFileStreamID *out) {
    AudioFileStreamID s;

    if (listener == NULL || packets == NULL || out == NULL)
        return kAudio_ParamError;
    s = calloc(1, sizeof(*s));
    if (s == NULL)
        return kAudio_MemFullError;
    s->magic = kStreamMagic;
    s->client = client;
    s->listener = listener;
    s->packets = packets;
    s->hint = hint;
    s->dataBytes = -1;
    *out = s;
    return noErr;
}

OSStatus AudioFileStreamClose(AudioFileStreamID s) {
    if (!alive(s))
        return kAudio_ParamError;
    free(s->pending);
    free(s->batch);
    s->magic = 0;
    free(s);
    return noErr;
}

static void announce(AudioFileStreamID s, AudioFileStreamPropertyID id) {
    AudioFileStreamPropertyFlags flags = 0;
    s->listener(s->client, s, id, &flags);
}

static void consume(AudioFileStreamID s, UInt32 n) {
    if (n > s->pendingSize)
        n = s->pendingSize;
    memmove(s->pending, s->pending + n, s->pendingSize - n);
    s->pendingSize -= n;
    s->position += n;
}

/* ---- The frames ---------------------------------------------------------------- */

static const UInt32 aacRates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                   16000, 12000, 11025, 8000, 7350 };

/* An ADTS frame at p: its length and header length, or 0. */
static UInt32 adtsFrame(const unsigned char *p, UInt32 have, UInt32 *header) {
    UInt32 length;

    if (have < 7 || p[0] != 0xFF || (p[1] & 0xF6) != 0xF0)
        return 0;
    if (((p[2] >> 2) & 0xF) >= 13)
        return 0;
    length = (UInt32)(p[3] & 3) << 11 | (UInt32)p[4] << 3 | p[5] >> 5;
    *header = (p[1] & 1) ? 7 : 9;
    return length > *header ? length : 0;
}

static const UInt16 mpegRates[3][3] = { { 44100, 48000, 32000 }, { 22050, 24000, 16000 }, { 11025, 12000, 8000 } };
static const UInt16 mpegBitRates[2][3][15] = {
    { { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
      { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } },
    { { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 } },
};

typedef struct {
    UInt32 length, rate, channels, samples, layer, bitRate;
} MPEGFrame;

/* An MPEG audio frame's header at p; false when it is not one. */
static Boolean mpegFrame(const unsigned char *p, UInt32 have, MPEGFrame *f) {
    UInt32 version, layer, bitIndex, rateIndex, padding, v;

    if (have < 4 || p[0] != 0xFF || (p[1] & 0xE0) != 0xE0)
        return false;
    version = (p[1] >> 3) & 3;      /* 3: MPEG-1, 2: MPEG-2, 0: MPEG-2.5 */
    layer = 4 - ((p[1] >> 1) & 3);  /* 1, 2 or 3 */
    bitIndex = p[2] >> 4;
    rateIndex = (p[2] >> 2) & 3;
    padding = (p[2] >> 1) & 1;
    if (version == 1 || layer == 4 || bitIndex == 0 || bitIndex == 15 || rateIndex == 3)
        return false;
    v = version == 3 ? 0 : 1;
    f->rate = mpegRates[version == 3 ? 0 : version == 2 ? 1 : 2][rateIndex];
    f->bitRate = mpegBitRates[v][layer - 1][bitIndex] * 1000;
    f->layer = layer;
    f->channels = (p[3] >> 6) == 3 ? 1 : 2;
    if (layer == 1) {
        f->samples = 384;
        f->length = (12 * f->bitRate / f->rate + padding) * 4;
    } else {
        f->samples = layer == 3 && v ? 576 : 1152;
        f->length = (layer == 3 && v ? 72 : 144) * f->bitRate / f->rate + padding;
    }
    return f->length > 4;
}

/* An ES descriptor holding an AudioSpecificConfig, as MPEG-4 files carry
   AAC's configuration. */
static void makeCookie(AudioFileStreamID s, UInt32 objectType, UInt32 rateIndex, UInt32 channels) {
    unsigned char *c = s->cookie;
    unsigned char asc[2] = { (unsigned char)(objectType << 3 | rateIndex >> 1),
                             (unsigned char)((rateIndex & 1) << 7 | channels << 3) };
    UInt32 n = 0;

    c[n++] = 3;                 /* ES descriptor */
    c[n++] = 3 + 2 + 15 + 2 + 2 + 3;
    c[n++] = 0; c[n++] = 0;     /* ES ID */
    c[n++] = 0;                 /* flags */
    c[n++] = 4;                 /* decoder configuration */
    c[n++] = 13 + 2 + 2;
    c[n++] = 0x40;              /* MPEG-4 audio */
    c[n++] = 0x15;              /* an audio stream */
    c[n++] = 0; c[n++] = 0; c[n++] = 0;                 /* buffer size */
    c[n++] = 0; c[n++] = 0; c[n++] = 0; c[n++] = 0;     /* maximum bit rate */
    c[n++] = 0; c[n++] = 0; c[n++] = 0; c[n++] = 0;     /* average bit rate */
    c[n++] = 5;                 /* decoder-specific: the AudioSpecificConfig */
    c[n++] = 2;
    c[n++] = asc[0];
    c[n++] = asc[1];
    c[n++] = 6;                 /* sync layer configuration */
    c[n++] = 1;
    c[n++] = 2;
    s->cookieSize = n;
}

/* ---- Handing packets over ------------------------------------------------------ */

static OSStatus addPacket(AudioFileStreamID s, const unsigned char *data, UInt32 size) {
    if (s->batchSize + size > s->batchCapacity) {
        UInt32 capacity = s->batchCapacity ? s->batchCapacity * 2 : 65536;
        unsigned char *grown;
        while (capacity < s->batchSize + size)
            capacity *= 2;
        grown = realloc(s->batch, capacity);
        if (grown == NULL)
            return kAudio_MemFullError;
        s->batch = grown;
        s->batchCapacity = capacity;
    }
    memcpy(s->batch + s->batchSize, data, size);
    s->descriptions[s->descriptionCount].mStartOffset = s->batchSize;
    s->descriptions[s->descriptionCount].mVariableFramesInPacket = 0;
    s->descriptions[s->descriptionCount].mDataByteSize = size;
    s->descriptionCount++;
    s->batchSize += size;
    s->packetCount++;
    s->packetBytes += size;
    if (size > s->maxPacket)
        s->maxPacket = size;
    return noErr;
}

static void flush(AudioFileStreamID s) {
    if (s->descriptionCount == 0)
        return;
    s->packets(s->client, s->batchSize, s->descriptionCount, s->batch, s->descriptions);
    s->batchSize = 0;
    s->descriptionCount = 0;
}

static void becomeReady(AudioFileStreamID s) {
    announce(s, kAudioFileStreamProperty_FileFormat);
    announce(s, kAudioFileStreamProperty_DataFormat);
    if (s->cookieSize > 0)
        announce(s, kAudioFileStreamProperty_MagicCookieData);
    announce(s, kAudioFileStreamProperty_DataOffset);
    if (s->dataBytes >= 0)
        announce(s, kAudioFileStreamProperty_AudioDataByteCount);
    s->ready = true;
    announce(s, kAudioFileStreamProperty_ReadyToProducePackets);
}

/* ---- Finding what the stream is --------------------------------------------------- */

/* An ID3 tag before MPEG audio: how long it is, once its header is here. */
static UInt32 id3Length(const unsigned char *p) {
    UInt32 size = (UInt32)(p[6] & 0x7F) << 21 | (UInt32)(p[7] & 0x7F) << 14 | (UInt32)(p[8] & 0x7F) << 7 | (p[9] & 0x7F);
    return 10 + size + ((p[5] & 0x10) ? 10 : 0);
}

/* Returns noErr when it knows (or needs more bytes), or the error that
   ends the stream. */
static OSStatus identify(AudioFileStreamID s) {
    const unsigned char *p = s->pending;
    UInt32 have = s->pendingSize, header;
    MPEGFrame m;

    if (have < 4)
        return noErr;
    if (memcmp(p, "RIFF", 4) == 0 || memcmp(p, "RF64", 4) == 0 || memcmp(p, "FORM", 4) == 0 ||
        memcmp(p, "caff", 4) == 0) {
        OSStatus status = _AudioFileParseHeader(p, have, &s->type, &s->format, &s->dataOffset, &s->dataBytes);
        if (status == kAudioFileInvalidFileError && have < kMaxHeader)
            return noErr;   /* more of the header to come */
        if (status != noErr)
            return status == kAudioFileUnsupportedDataFormatError ? kAudioFileStreamError_UnsupportedDataFormat
                                                                  : kAudioFileStreamError_InvalidFile;
        s->dataOffset += s->position;
        s->kind = kKindPCM;
        becomeReady(s);
        consume(s, (UInt32)(s->dataOffset - s->position));
        return noErr;
    }
    if (memcmp(p, "ID3", 3) == 0) {
        if (have < 10)
            return noErr;
        s->skip = id3Length(p);
        return noErr;
    }
    /* A frame sync, found where the data starts (or after junk). */
    for (UInt32 i = 0; i + 4 <= have; i++) {
        UInt32 adts = s->hint != kAudioFileMP3Type ? adtsFrame(p + i, have - i, &header) : 0;
        if (adts > 0) {
            UInt32 objectType = (p[i + 2] >> 6) + 1, rateIndex = (p[i + 2] >> 2) & 0xF;
            UInt32 channels = (UInt32)(p[i + 2] & 1) << 2 | p[i + 3] >> 6;
            consume(s, i);
            s->kind = kKindADTS;
            s->type = kAudioFileAAC_ADTSType;
            memset(&s->format, 0, sizeof(s->format));
            s->format.mSampleRate = aacRates[rateIndex];
            s->format.mFormatID = kAudioFormatMPEG4AAC;
            s->format.mFormatFlags = objectType;
            s->format.mFramesPerPacket = 1024;
            s->format.mChannelsPerFrame = channels;
            makeCookie(s, objectType, rateIndex, channels);
            s->dataOffset = s->position;
            becomeReady(s);
            return noErr;
        }
        if (s->hint != kAudioFileAAC_ADTSType && mpegFrame(p + i, have - i, &m) && m.layer != 0 &&
            (p[i + 1] & 0x06) != 0) {
            consume(s, i);
            s->kind = kKindMPEG;
            s->type = m.layer == 3 ? kAudioFileMP3Type : m.layer == 2 ? kAudioFileMP2Type : kAudioFileMP1Type;
            memset(&s->format, 0, sizeof(s->format));
            s->format.mSampleRate = m.rate;
            s->format.mFormatID = m.layer == 3 ? kAudioFormatMPEGLayer3
                                  : m.layer == 2 ? kAudioFormatMPEGLayer2 : kAudioFormatMPEGLayer1;
            s->format.mFramesPerPacket = m.samples;
            s->format.mChannelsPerFrame = m.channels;
            s->bitRate = m.bitRate;
            s->dataOffset = s->position;
            becomeReady(s);
            return noErr;
        }
    }
    if (have > kMaxHeader)
        return kAudioFileStreamError_UnsupportedFileType;
    return noErr;
}

/* ---- Parsing ---------------------------------------------------------------- */

static OSStatus produce(AudioFileStreamID s, Boolean flushAll) {
    OSStatus status = noErr;

    switch (s->kind) {
    case kKindPCM: {
        UInt32 bpf = s->format.mBytesPerFrame;
        UInt32 n = s->pendingSize / bpf;
        if (s->dataBytes >= 0 && (SInt64)n * bpf > s->dataBytes - s->dataSeen)
            n = (UInt32)((s->dataBytes - s->dataSeen) / bpf);
        if (n > 0) {
            s->packets(s->client, n * bpf, n, s->pending, NULL);
            s->packetCount += n;
            s->packetBytes += (UInt64)n * bpf;
            s->dataSeen += (SInt64)n * bpf;
            consume(s, n * bpf);
        }
        if (s->dataBytes >= 0 && s->dataSeen >= s->dataBytes)
            consume(s, s->pendingSize);     /* what follows the data */
        break;
    }
    case kKindADTS:
        for (;;) {
            UInt32 header, length = adtsFrame(s->pending, s->pendingSize, &header);
            if (length == 0) {
                /* Lost sync (after a discontinuity): look for the next frame. */
                UInt32 i = 1;
                if (s->pendingSize < 7)
                    break;
                while (i + 7 <= s->pendingSize && adtsFrame(s->pending + i, s->pendingSize - i, &header) == 0)
                    i++;
                consume(s, i);
                continue;
            }
            if (s->pendingSize < length)
                break;
            status = addPacket(s, s->pending + header, length - header);
            consume(s, length);
            if (status != noErr || s->descriptionCount == kBatch)
                flush(s);
            if (status != noErr)
                return status;
        }
        break;
    case kKindMPEG:
        for (;;) {
            MPEGFrame m;
            if (!mpegFrame(s->pending, s->pendingSize, &m)) {
                UInt32 i = 1;
                if (s->pendingSize < 4)
                    break;
                while (i + 4 <= s->pendingSize && !mpegFrame(s->pending + i, s->pendingSize - i, &m))
                    i++;
                consume(s, i);
                continue;
            }
            if (s->pendingSize < m.length)
                break;
            status = addPacket(s, s->pending, m.length);
            consume(s, m.length);
            if (status != noErr || s->descriptionCount == kBatch)
                flush(s);
            if (status != noErr)
                return status;
        }
        break;
    default:
        break;
    }
    if (flushAll)
        flush(s);
    return noErr;
}

OSStatus AudioFileStreamParseBytes(AudioFileStreamID s, UInt32 size, const void *data, AudioFileStreamParseFlags flags) {
    const unsigned char *bytes = data;
    OSStatus status;

    if (!alive(s) || (data == NULL && size > 0))
        return kAudio_ParamError;
    if (s->failed)
        return kAudioFileStreamError_InvalidFile;
    if (flags & kAudioFileStreamParseFlag_Discontinuity) {
        /* What was kept belongs to before the gap. */
        if (s->kind != kKindUnknown) {
            s->position += s->pendingSize;
            s->pendingSize = 0;
        }
    }
    /* An ID3 tag is dropped as it comes. */
    while (s->skip > 0 && size > 0) {
        UInt32 n = s->pendingSize > 0 ? (s->pendingSize < s->skip ? s->pendingSize : s->skip) : 0;
        if (n > 0) {
            consume(s, n);
            s->skip -= n;
            continue;
        }
        n = size < s->skip ? size : s->skip;
        bytes += n;
        size -= n;
        s->skip -= n;
        s->position += n;
    }
    if (s->pendingSize + size > s->pendingCapacity) {
        UInt32 capacity = s->pendingCapacity ? s->pendingCapacity : 65536;
        unsigned char *grown;
        while (capacity < s->pendingSize + size)
            capacity *= 2;
        grown = realloc(s->pending, capacity);
        if (grown == NULL)
            return kAudio_MemFullError;
        s->pending = grown;
        s->pendingCapacity = capacity;
    }
    memcpy(s->pending + s->pendingSize, bytes, size);
    s->pendingSize += size;

    while (s->kind == kKindUnknown) {
        UInt32 before = s->pendingSize;
        SInt64 at = s->position;
        status = identify(s);
        if (status != noErr) {
            s->failed = true;
            return status;
        }
        if (s->skip > 0) {     /* an ID3 tag found: drop what of it is here */
            UInt32 n = s->pendingSize < s->skip ? s->pendingSize : s->skip;
            consume(s, n);
            s->skip -= n;
            if (s->skip > 0)
                return noErr;   /* the rest of the tag is still to come */
            continue;
        }
        if (s->kind == kKindUnknown && before == s->pendingSize && at == s->position)
            return noErr;       /* more bytes needed */
    }
    return produce(s, true);
}

/* ---- Seeking and properties --------------------------------------------------- */

OSStatus AudioFileStreamSeek(AudioFileStreamID s, SInt64 packet, SInt64 *outByteOffset,
                             AudioFileStreamSeekFlags *ioFlags) {
    if (!alive(s) || outByteOffset == NULL || ioFlags == NULL)
        return kAudio_ParamError;
    if (!s->ready)
        return kAudioFileStreamError_DataUnavailable;
    if (packet < 0)
        return kAudioFileStreamError_InvalidPacketOffset;
    if (s->kind == kKindPCM) {
        *outByteOffset = s->dataOffset + packet * s->format.mBytesPerPacket;
        *ioFlags = 0;
    } else {
        double average = s->packetCount > 0 ? (double)s->packetBytes / s->packetCount : 0;
        if (average <= 0)
            return kAudioFileStreamError_DataUnavailable;
        *outByteOffset = s->dataOffset + (SInt64)(packet * average);
        *ioFlags = kAudioFileStreamSeekFlag_OffsetIsEstimated;
    }
    /* The bytes given next start there. */
    s->pendingSize = 0;
    s->position = *outByteOffset;
    s->dataSeen = *outByteOffset - s->dataOffset;
    s->batchSize = 0;
    s->descriptionCount = 0;
    return noErr;
}

static UInt32 propertySize(AudioFileStreamID s, AudioFileStreamPropertyID id) {
    switch (id) {
    case kAudioFileStreamProperty_ReadyToProducePackets:
    case kAudioFileStreamProperty_FileFormat:
    case kAudioFileStreamProperty_MaximumPacketSize:
    case kAudioFileStreamProperty_PacketSizeUpperBound:
    case kAudioFileStreamProperty_BitRate:
        return sizeof(UInt32);
    case kAudioFileStreamProperty_DataFormat:
        return sizeof(AudioStreamBasicDescription);
    case kAudioFileStreamProperty_FormatList:
        return sizeof(AudioFormatListItem);
    case kAudioFileStreamProperty_MagicCookieData:
        return s->cookieSize;
    case kAudioFileStreamProperty_DataOffset:
        return sizeof(SInt64);
    case kAudioFileStreamProperty_AudioDataByteCount:
    case kAudioFileStreamProperty_AudioDataPacketCount:
        return sizeof(UInt64);
    case kAudioFileStreamProperty_AverageBytesPerPacket:
        return sizeof(Float64);
    case kAudioFileStreamProperty_PacketToFrame:
    case kAudioFileStreamProperty_FrameToPacket:
        return sizeof(AudioFramePacketTranslation);
    case kAudioFileStreamProperty_PacketToByte:
    case kAudioFileStreamProperty_ByteToPacket:
        return sizeof(AudioBytePacketTranslation);
    default:
        return 0;
    }
}

OSStatus AudioFileStreamGetPropertyInfo(AudioFileStreamID s, AudioFileStreamPropertyID id, UInt32 *outSize,
                                        Boolean *outWritable) {
    UInt32 size;

    if (!alive(s))
        return kAudio_ParamError;
    size = propertySize(s, id);
    if (size == 0)
        return id == kAudioFileStreamProperty_MagicCookieData ? kAudioFileStreamError_ValueUnknown
                                                              : kAudioFileStreamError_UnsupportedProperty;
    if (outSize != NULL)
        *outSize = size;
    if (outWritable != NULL)
        *outWritable = false;
    return noErr;
}

OSStatus AudioFileStreamGetProperty(AudioFileStreamID s, AudioFileStreamPropertyID id, UInt32 *ioSize, void *data) {
    UInt32 size;
    double average;

    if (!alive(s) || ioSize == NULL || data == NULL)
        return kAudio_ParamError;
    size = propertySize(s, id);
    if (size == 0)
        return id == kAudioFileStreamProperty_MagicCookieData ? kAudioFileStreamError_ValueUnknown
                                                              : kAudioFileStreamError_UnsupportedProperty;
    if (*ioSize < size)
        return kAudioFileStreamError_BadPropertySize;
    if (!s->ready && id != kAudioFileStreamProperty_ReadyToProducePackets)
        return kAudioFileStreamError_DataUnavailable;
    average = s->packetCount > 0 ? (double)s->packetBytes / s->packetCount
                                 : (s->kind == kKindPCM ? s->format.mBytesPerPacket : 0);
    switch (id) {
    case kAudioFileStreamProperty_ReadyToProducePackets:
        *(UInt32 *)data = s->ready;
        break;
    case kAudioFileStreamProperty_FileFormat:
        *(UInt32 *)data = s->type;
        break;
    case kAudioFileStreamProperty_MaximumPacketSize:
    case kAudioFileStreamProperty_PacketSizeUpperBound:
        if (s->kind == kKindPCM)
            *(UInt32 *)data = s->format.mBytesPerPacket;
        else if (s->kind == kKindADTS)
            *(UInt32 *)data = s->maxPacket > 0 ? s->maxPacket : 8192;   /* 13 bits of frame length */
        else
            *(UInt32 *)data = s->maxPacket > 0 ? s->maxPacket : 2881;   /* a layer 1 to 3 frame's most */
        break;
    case kAudioFileStreamProperty_BitRate:
        if (s->kind == kKindPCM)
            *(UInt32 *)data = (UInt32)(s->format.mSampleRate * s->format.mBytesPerFrame * 8);
        else if (s->bitRate > 0 && s->packetCount == 0)
            *(UInt32 *)data = s->bitRate;
        else if (average > 0)
            *(UInt32 *)data = (UInt32)(average * 8 * s->format.mSampleRate / s->format.mFramesPerPacket);
        else
            return kAudioFileStreamError_ValueUnknown;
        break;
    case kAudioFileStreamProperty_DataFormat:
        *(AudioStreamBasicDescription *)data = s->format;
        break;
    case kAudioFileStreamProperty_FormatList: {
        AudioFormatListItem item;
        item.mASBD = s->format;
        item.mChannelLayoutTag = s->format.mChannelsPerFrame == 1 ? kAudioChannelLayoutTag_Mono
                                 : s->format.mChannelsPerFrame == 2 ? kAudioChannelLayoutTag_Stereo
                                 : kAudioChannelLayoutTag_DiscreteInOrder | s->format.mChannelsPerFrame;
        *(AudioFormatListItem *)data = item;
        break;
    }
    case kAudioFileStreamProperty_MagicCookieData:
        memcpy(data, s->cookie, s->cookieSize);
        break;
    case kAudioFileStreamProperty_DataOffset:
        *(SInt64 *)data = s->dataOffset;
        break;
    case kAudioFileStreamProperty_AudioDataByteCount:
        if (s->dataBytes < 0)
            return kAudioFileStreamError_ValueUnknown;
        *(UInt64 *)data = (UInt64)s->dataBytes;
        break;
    case kAudioFileStreamProperty_AudioDataPacketCount:
        if (s->dataBytes < 0 || s->kind != kKindPCM)
            return kAudioFileStreamError_ValueUnknown;
        *(UInt64 *)data = (UInt64)(s->dataBytes / s->format.mBytesPerPacket);
        break;
    case kAudioFileStreamProperty_AverageBytesPerPacket:
        if (average <= 0)
            return kAudioFileStreamError_ValueUnknown;
        *(Float64 *)data = average;
        break;
    case kAudioFileStreamProperty_PacketToFrame: {
        AudioFramePacketTranslation *t = data;
        t->mFrame = t->mPacket * s->format.mFramesPerPacket;
        t->mFrameOffsetInPacket = 0;
        break;
    }
    case kAudioFileStreamProperty_FrameToPacket: {
        AudioFramePacketTranslation *t = data;
        t->mPacket = t->mFrame / s->format.mFramesPerPacket;
        t->mFrameOffsetInPacket = (UInt32)(t->mFrame % s->format.mFramesPerPacket);
        break;
    }
    case kAudioFileStreamProperty_PacketToByte: {
        AudioBytePacketTranslation *t = data;
        if (average <= 0)
            return kAudioFileStreamError_ValueUnknown;
        t->mByte = (SInt64)(t->mPacket * average);
        t->mFlags = s->kind == kKindPCM ? 0 : 1;    /* kBytePacketTranslationFlag_IsEstimate */
        break;
    }
    case kAudioFileStreamProperty_ByteToPacket: {
        AudioBytePacketTranslation *t = data;
        if (average <= 0)
            return kAudioFileStreamError_ValueUnknown;
        t->mPacket = (SInt64)(t->mByte / average);
        t->mByteOffsetInPacket = (UInt32)(t->mByte - (SInt64)(t->mPacket * average));
        t->mFlags = s->kind == kKindPCM ? 0 : 1;
        break;
    }
    }
    *ioSize = size;
    return noErr;
}

OSStatus AudioFileStreamSetProperty(AudioFileStreamID s, AudioFileStreamPropertyID id, UInt32 size, const void *data) {
    (void)size;
    (void)data;
    if (!alive(s))
        return kAudio_ParamError;
    return propertySize(s, id) != 0 ? kAudioFileStreamError_IllegalOperation
                                    : kAudioFileStreamError_UnsupportedProperty;
}
