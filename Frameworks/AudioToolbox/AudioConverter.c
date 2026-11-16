/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio converters for linear PCM: any sample format the units read, from
 * one channel count to another (or through a channel map), and from one
 * rate to another with speexdsp's resampler. Input is taken in float, in
 * the destination's channels, and kept between calls until it is used up.
 * Compressed formats come with the codecs (AU4).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioConverter.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "speexdsp/speex_resampler.h"

#define kConverterMagic 0x41434e56u    /* 'ACNV' */
#define kChunkFrames    1024
#define kOutFrames      4096
#define kMaxBuffers     32

struct OpaqueAudioConverter {
    uint32_t magic;
    AudioStreamBasicDescription src, dst;
    SInt32 *map;                /* the destination's channels, or NULL */
    SpeexResamplerState *resampler;
    UInt32 quality, complexity, codecQuality, primeMethod;

    float *in;                  /* one input chunk, in the source's channels */
    float *pending;             /* input not yet converted, in the destination's */
    UInt32 pendingStart, pendingCount, pendingCapacity;
    float *out;                 /* resampled, kOutFrames long */
    Boolean ended, drained;

    AudioBufferList *inList;    /* what the input procedure fills */
    AudioBufferList *view;      /* a chunk of the input, taken in turn */

    ATCodec *codec;             /* a compressed source's decoder (ATCodec.c) */
};

static Boolean alive(AudioConverterRef c) {
    return c != NULL && c->magic == kConverterMagic;
}

static int speexQuality(UInt32 quality) {
    if (quality >= kAudioConverterQuality_Max)
        return 10;
    if (quality >= kAudioConverterQuality_High)
        return 7;
    if (quality >= kAudioConverterQuality_Medium)
        return 5;
    if (quality >= kAudioConverterQuality_Low)
        return 3;
    return 1;
}

static Boolean resampling(AudioConverterRef c) {
    return fabs(c->src.mSampleRate - c->dst.mSampleRate) > 1e-6;
}

static void freeConverter(AudioConverterRef c) {
    _ATCodecDispose(c->codec);
    if (c->resampler != NULL)
        speex_resampler_destroy(c->resampler);
    free(c->map);
    free(c->in);
    free(c->pending);
    free(c->out);
    free(c->inList);
    free(c->view);
    c->magic = 0;
    free(c);
}

static OSStatus makeResampler(AudioConverterRef c) {
    int err = 0;

    if (c->resampler != NULL) {
        speex_resampler_destroy(c->resampler);
        c->resampler = NULL;
    }
    if (!resampling(c))
        return noErr;
    /* speexdsp takes whole rates: as a fraction of the two in hundredths
       when either is not whole. */
    if (c->src.mSampleRate == floor(c->src.mSampleRate) && c->dst.mSampleRate == floor(c->dst.mSampleRate))
        c->resampler = speex_resampler_init(c->dst.mChannelsPerFrame, (spx_uint32_t)c->src.mSampleRate,
                                            (spx_uint32_t)c->dst.mSampleRate, speexQuality(c->quality), &err);
    else
        c->resampler = speex_resampler_init_frac(c->dst.mChannelsPerFrame,
                                                 (spx_uint32_t)lround(c->src.mSampleRate * 100),
                                                 (spx_uint32_t)lround(c->dst.mSampleRate * 100),
                                                 (spx_uint32_t)lround(c->src.mSampleRate),
                                                 (spx_uint32_t)lround(c->dst.mSampleRate),
                                                 speexQuality(c->quality), &err);
    if (c->resampler == NULL)
        return kAudioConverterErr_UnspecifiedError;
    if (c->primeMethod != kConverterPrimeMethod_None)
        speex_resampler_skip_zeros(c->resampler);
    return noErr;
}

OSStatus AudioConverterNew(const AudioStreamBasicDescription *inSource, const AudioStreamBasicDescription *inDest,
                           AudioConverterRef *out) {
    AudioConverterRef c;
    OSStatus status;

    if (inSource == NULL || inDest == NULL || out == NULL)
        return kAudio_ParamError;
    *out = NULL;
    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return kAudio_MemFullError;
    c->src = *inSource;
    c->dst = *inDest;
    /* A rate of 0 is the other side's. */
    if (c->src.mSampleRate == 0)
        c->src.mSampleRate = c->dst.mSampleRate;
    if (c->dst.mSampleRate == 0)
        c->dst.mSampleRate = c->src.mSampleRate;
    /* A compressed source to linear PCM: decoded (ATCodec.c). */
    if (c->src.mFormatID != kAudioFormatLinearPCM && c->dst.mFormatID == kAudioFormatLinearPCM) {
        c->codec = _ATCodecNew(&c->src, &c->dst, &status);
        c->inList = calloc(1, offsetof(AudioBufferList, mBuffers) + kMaxBuffers * sizeof(AudioBuffer));
        if (c->codec == NULL || c->inList == NULL) {
            OSStatus failed = c->codec == NULL ? status : kAudio_MemFullError;
            _ATCodecDispose(c->codec);
            free(c->inList);
            free(c);
            return failed;
        }
        c->magic = kConverterMagic;
        c->quality = c->codecQuality = kAudioConverterQuality_Medium;
        c->complexity = kAudioConverterSampleRateConverterComplexity_Normal;
        c->primeMethod = kConverterPrimeMethod_Normal;
        *out = c;
        return noErr;
    }
    if (!_AUFormatIsSupported(&c->src) || !_AUFormatIsSupported(&c->dst)) {
        free(c);
        return kAudioConverterErr_FormatNotSupported;
    }
    c->magic = kConverterMagic;
    c->quality = kAudioConverterQuality_Medium;
    c->complexity = kAudioConverterSampleRateConverterComplexity_Normal;
    c->codecQuality = kAudioConverterQuality_Medium;
    c->primeMethod = kConverterPrimeMethod_Normal;
    c->in = malloc(kChunkFrames * c->src.mChannelsPerFrame * sizeof(float));
    c->out = malloc(kOutFrames * c->dst.mChannelsPerFrame * sizeof(float));
    c->inList = calloc(1, offsetof(AudioBufferList, mBuffers) + kMaxBuffers * sizeof(AudioBuffer));
    c->view = calloc(1, offsetof(AudioBufferList, mBuffers) + kMaxBuffers * sizeof(AudioBuffer));
    if (c->in == NULL || c->out == NULL || c->inList == NULL || c->view == NULL) {
        freeConverter(c);
        return kAudio_MemFullError;
    }
    status = makeResampler(c);
    if (status != noErr) {
        freeConverter(c);
        return status;
    }
    *out = c;
    return noErr;
}

OSStatus AudioConverterNewSpecific(const AudioStreamBasicDescription *inSource,
                                   const AudioStreamBasicDescription *inDest, UInt32 count,
                                   const void *descriptions, AudioConverterRef *out) {
    return AudioConverterNew(inSource, inDest, out);
}

OSStatus AudioConverterDispose(AudioConverterRef c) {
    if (!alive(c))
        return kAudio_ParamError;
    freeConverter(c);
    return noErr;
}

OSStatus AudioConverterReset(AudioConverterRef c) {
    if (!alive(c))
        return kAudio_ParamError;
    if (c->codec != NULL) {
        _ATCodecReset(c->codec);
        return noErr;
    }
    c->pendingStart = c->pendingCount = 0;
    c->ended = c->drained = false;
    if (c->resampler != NULL) {
        speex_resampler_reset_mem(c->resampler);
        if (c->primeMethod != kConverterPrimeMethod_None)
            speex_resampler_skip_zeros(c->resampler);
    }
    return noErr;
}

/* ---- Properties ------------------------------------------------------------ */

static UInt32 layoutBytes(UInt32 channels) {
    return (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) + channels * sizeof(AudioChannelDescription));
}

static UInt32 propertySize(AudioConverterRef c, AudioConverterPropertyID id, Boolean *writable) {
    *writable = false;
    if (c->codec != NULL) {
        /* A decoder's: its cookie and layouts; no channel map or
           resampler of this file's. */
        switch (id) {
        case kAudioConverterDecompressionMagicCookie: {
            UInt32 size;
            *writable = true;
            _ATCodecCookie(c->codec, &size);
            return size > 0 ? size : 1;
        }
        case kAudioConverterInputChannelLayout:
        case kAudioConverterOutputChannelLayout:
            return layoutBytes(_ATCodecChannels(c->codec));
        case kAudioConverterChannelMap:
        case kAudioConverterPropertyCalculateInputBufferSize:
        case kAudioConverterPropertyCalculateOutputBufferSize:
            return 0;
        default:
            break;
        }
    }
    switch (id) {
    case kAudioConverterCurrentInputStreamDescription:
    case kAudioConverterCurrentOutputStreamDescription:
        return sizeof(AudioStreamBasicDescription);
    case kAudioConverterPropertyMinimumInputBufferSize:
    case kAudioConverterPropertyMinimumOutputBufferSize:
    case kAudioConverterPropertyMaximumInputPacketSize:
    case kAudioConverterPropertyMaximumOutputPacketSize:
    case kAudioConverterPropertyCalculateInputBufferSize:
    case kAudioConverterPropertyCalculateOutputBufferSize:
        return sizeof(UInt32);
    case kAudioConverterSampleRateConverterQuality:
    case kAudioConverterSampleRateConverterComplexity:
    case kAudioConverterCodecQuality:
    case kAudioConverterPrimeMethod:
        *writable = true;
        return sizeof(UInt32);
    case kAudioConverterPrimeInfo:
        *writable = true;
        return sizeof(AudioConverterPrimeInfo);
    case kAudioConverterChannelMap:
        *writable = true;
        return c->dst.mChannelsPerFrame * sizeof(SInt32);
    default:
        return 0;
    }
}

OSStatus AudioConverterGetPropertyInfo(AudioConverterRef c, AudioConverterPropertyID id, UInt32 *outSize,
                                       Boolean *outWritable) {
    Boolean writable;
    UInt32 size;

    if (!alive(c))
        return kAudio_ParamError;
    size = propertySize(c, id, &writable);
    if (size == 0)
        return kAudioConverterErr_PropertyNotSupported;
    if (outSize != NULL)
        *outSize = size;
    if (outWritable != NULL)
        *outWritable = writable;
    return noErr;
}

static UInt32 leadingFrames(AudioConverterRef c) {
    return c->resampler != NULL ? (UInt32)speex_resampler_get_input_latency(c->resampler) : 0;
}

OSStatus AudioConverterGetProperty(AudioConverterRef c, AudioConverterPropertyID id, UInt32 *ioSize,
                                   void *data) {
    Boolean writable;
    UInt32 size, v;
    double ratio;

    if (!alive(c) || ioSize == NULL || data == NULL)
        return kAudio_ParamError;
    size = propertySize(c, id, &writable);
    if (size == 0)
        return kAudioConverterErr_PropertyNotSupported;
    if (c->codec != NULL && id == kAudioConverterDecompressionMagicCookie) {
        UInt32 have;
        const void *cookie = _ATCodecCookie(c->codec, &have);
        if (*ioSize < have)
            return kAudioConverterErr_BadPropertySizeError;
        if (have > 0)
            memcpy(data, cookie, have);
        *ioSize = have;
        return noErr;
    }
    if (*ioSize < size)
        return kAudioConverterErr_BadPropertySizeError;
    if (c->codec != NULL && (id == kAudioConverterInputChannelLayout || id == kAudioConverterOutputChannelLayout)) {
        /* Each channel described, in the order they come out. */
        AudioChannelLayout *l = data;
        UInt32 n = _ATCodecChannels(c->codec);
        memset(l, 0, size);
        l->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
        l->mNumberChannelDescriptions = n;
        for (UInt32 i = 0; i < n; i++)
            l->mChannelDescriptions[i].mChannelLabel = _ATCodecChannelLabel(c->codec, i);
        *ioSize = size;
        return noErr;
    }
    if (c->codec != NULL && (id == kAudioConverterPropertyMinimumInputBufferSize ||
                             id == kAudioConverterPropertyMaximumInputPacketSize)) {
        /* A compressed packet's bound: AAC's is 6144 bits a channel. */
        *(UInt32 *)data = c->src.mBytesPerPacket > 0 ? c->src.mBytesPerPacket
                                                     : 768 * (c->src.mChannelsPerFrame ? c->src.mChannelsPerFrame : 2);
        *ioSize = size;
        return noErr;
    }
    if (c->codec != NULL && id == kAudioConverterPrimeInfo) {
        AudioConverterPrimeInfo info = { 0, 0 };
        memcpy(data, &info, size);
        *ioSize = size;
        return noErr;
    }
    ratio = c->src.mSampleRate / c->dst.mSampleRate;
    switch (id) {
    case kAudioConverterCurrentInputStreamDescription:
        memcpy(data, &c->src, size);
        break;
    case kAudioConverterCurrentOutputStreamDescription:
        memcpy(data, &c->dst, size);
        break;
    case kAudioConverterPropertyMinimumInputBufferSize:
    case kAudioConverterPropertyMaximumInputPacketSize:
        *(UInt32 *)data = c->src.mBytesPerFrame;
        break;
    case kAudioConverterPropertyMinimumOutputBufferSize:
    case kAudioConverterPropertyMaximumOutputPacketSize:
        *(UInt32 *)data = c->dst.mBytesPerFrame;
        break;
    case kAudioConverterPropertyCalculateInputBufferSize:
        /* In: the output's bytes; out: the input's for them. */
        v = *(UInt32 *)data / c->dst.mBytesPerFrame;
        *(UInt32 *)data = (UInt32)ceil(v * ratio) * c->src.mBytesPerFrame;
        break;
    case kAudioConverterPropertyCalculateOutputBufferSize:
        v = *(UInt32 *)data / c->src.mBytesPerFrame;
        *(UInt32 *)data = (UInt32)floor(v / ratio) * c->dst.mBytesPerFrame;
        break;
    case kAudioConverterSampleRateConverterQuality:
        *(UInt32 *)data = c->quality;
        break;
    case kAudioConverterSampleRateConverterComplexity:
        *(UInt32 *)data = c->complexity;
        break;
    case kAudioConverterCodecQuality:
        *(UInt32 *)data = c->codecQuality;
        break;
    case kAudioConverterPrimeMethod:
        *(UInt32 *)data = c->primeMethod;
        break;
    case kAudioConverterPrimeInfo: {
        AudioConverterPrimeInfo info = { c->primeMethod == kConverterPrimeMethod_None ? 0 : leadingFrames(c), 0 };
        memcpy(data, &info, size);
        break;
    }
    case kAudioConverterChannelMap:
        for (UInt32 i = 0; i < c->dst.mChannelsPerFrame; i++)
            ((SInt32 *)data)[i] = c->map != NULL ? c->map[i]
                                  : (c->src.mChannelsPerFrame == 1 ? 0
                                     : (i < c->src.mChannelsPerFrame ? (SInt32)i : -1));
        break;
    }
    *ioSize = size;
    return noErr;
}

OSStatus AudioConverterSetProperty(AudioConverterRef c, AudioConverterPropertyID id, UInt32 inSize,
                                   const void *data) {
    Boolean writable;
    UInt32 size;

    if (!alive(c) || data == NULL)
        return kAudio_ParamError;
    if (c->codec != NULL && id == kAudioConverterDecompressionMagicCookie)
        return _ATCodecSetCookie(c->codec, data, inSize);
    size = propertySize(c, id, &writable);
    if (size == 0)
        return kAudioConverterErr_PropertyNotSupported;
    if (!writable)
        return kAudioConverterErr_OperationNotSupported;
    if (inSize < size)
        return kAudioConverterErr_BadPropertySizeError;
    if (c->codec != NULL) {
        /* The rest a decoder takes and keeps, with nothing to change. */
        if (id == kAudioConverterCodecQuality)
            c->codecQuality = *(const UInt32 *)data;
        else if (id == kAudioConverterSampleRateConverterQuality)
            c->quality = *(const UInt32 *)data;
        else if (id == kAudioConverterSampleRateConverterComplexity)
            c->complexity = *(const UInt32 *)data;
        else if (id == kAudioConverterPrimeMethod)
            c->primeMethod = *(const UInt32 *)data;
        return noErr;
    }
    switch (id) {
    case kAudioConverterSampleRateConverterQuality:
        c->quality = *(const UInt32 *)data;
        if (c->resampler != NULL)
            speex_resampler_set_quality(c->resampler, speexQuality(c->quality));
        break;
    case kAudioConverterSampleRateConverterComplexity:
        c->complexity = *(const UInt32 *)data;
        break;
    case kAudioConverterCodecQuality:
        c->codecQuality = *(const UInt32 *)data;
        break;
    case kAudioConverterPrimeMethod:
        if (*(const UInt32 *)data > kConverterPrimeMethod_None)
            return kAudio_ParamError;
        c->primeMethod = *(const UInt32 *)data;
        return makeResampler(c);
    case kAudioConverterPrimeInfo:
        break;  /* the priming is the resampler's own */
    case kAudioConverterChannelMap: {
        SInt32 *map = malloc(size);
        if (map == NULL)
            return kAudio_MemFullError;
        memcpy(map, data, size);
        for (UInt32 i = 0; i < c->dst.mChannelsPerFrame; i++)
            if (map[i] >= (SInt32)c->src.mChannelsPerFrame) {
                free(map);
                return kAudio_ParamError;
            }
        free(c->map);
        c->map = map;
        break;
    }
    }
    return noErr;
}

/* ---- Converting ------------------------------------------------------------- */

static Boolean growPending(AudioConverterRef c, UInt32 frames) {
    float *p;

    if (frames <= c->pendingCapacity)
        return true;
    p = realloc(c->pending, (size_t)frames * c->dst.mChannelsPerFrame * sizeof(float));
    if (p == NULL)
        return false;
    c->pending = p;
    c->pendingCapacity = frames;
    return true;
}

/* Takes frames of the source into pending (which is empty), a chunk at a
   time through c->in. */
static OSStatus takeInput(AudioConverterRef c, const AudioBufferList *list, UInt32 frames) {
    UInt32 done = 0;
    AudioBufferList *view = c->view;   /* never the list it views */
    UInt32 planar = (c->src.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;

    if (!growPending(c, frames))
        return kAudio_MemFullError;
    c->pendingStart = 0;
    c->pendingCount = 0;
    while (done < frames) {
        UInt32 n = frames - done < kChunkFrames ? frames - done : kChunkFrames;
        UInt32 count = list->mNumberBuffers < kMaxBuffers ? list->mNumberBuffers : kMaxBuffers;

        /* A view of the buffers from frame done on. */
        view->mNumberBuffers = count;
        for (UInt32 b = 0; b < count; b++) {
            UInt32 step = planar ? c->src.mBitsPerChannel / 8 : c->src.mBytesPerFrame;
            UInt32 skip = done * step;
            view->mBuffers[b].mNumberChannels = list->mBuffers[b].mNumberChannels;
            view->mBuffers[b].mData = list->mBuffers[b].mData != NULL ? (char *)list->mBuffers[b].mData + skip : NULL;
            view->mBuffers[b].mDataByteSize = list->mBuffers[b].mDataByteSize > skip
                                              ? list->mBuffers[b].mDataByteSize - skip : 0;
        }
        _AUToFloat(&c->src, view, n, c->in, c->src.mChannelsPerFrame);
        _AUMapChannels(c->in, c->src.mChannelsPerFrame, c->pending + (size_t)done * c->dst.mChannelsPerFrame,
                       c->dst.mChannelsPerFrame, n, c->map);
        done += n;
    }
    c->pendingCount = frames;
    return noErr;
}

static UInt32 framesIn(const AudioStreamBasicDescription *f, const AudioBufferList *list) {
    UInt32 frames = UINT32_MAX;
    Boolean planar = (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    UInt32 step = planar ? f->mBitsPerChannel / 8 : f->mBytesPerFrame;

    if (list->mNumberBuffers == 0)
        return 0;
    for (UInt32 b = 0; b < (planar ? list->mNumberBuffers : 1); b++) {
        UInt32 n = list->mBuffers[b].mDataByteSize / step;
        if (n < frames)
            frames = n;
    }
    return frames;
}

static void setOutputSizes(AudioConverterRef c, AudioBufferList *out, UInt32 frames) {
    Boolean planar = (c->dst.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    UInt32 step = planar ? c->dst.mBitsPerChannel / 8 : c->dst.mBytesPerFrame;

    for (UInt32 b = 0; b < out->mNumberBuffers; b++)
        out->mBuffers[b].mDataByteSize = frames * step;
}

OSStatus AudioConverterFillComplexBuffer(AudioConverterRef c, AudioConverterComplexInputDataProc proc,
                                         void *userData, UInt32 *ioPackets, AudioBufferList *out,
                                         AudioStreamPacketDescription *outDescriptions) {
    UInt32 want, produced = 0, capacity, ch, guard = 0;
    OSStatus status = noErr;
    Boolean stop = false;

    if (!alive(c) || proc == NULL || ioPackets == NULL || out == NULL)
        return kAudio_ParamError;
    if (c->codec != NULL)
        return _ATCodecFill(c->codec, c, proc, userData, ioPackets, out, c->inList);
    ch = c->dst.mChannelsPerFrame;
    want = *ioPackets;
    capacity = framesIn(&c->dst, out);
    if (want > capacity)
        want = capacity;

    while (produced < want && guard++ < 100000) {
        if (c->pendingCount == 0) {
            UInt32 n, have;
            AudioStreamPacketDescription *descriptions = NULL;

            if (stop)
                break;
            if (c->ended) {
                /* At the end, the resampler's last frames: push zeros
                   through it once. */
                if (c->resampler == NULL || c->drained)
                    break;
                c->drained = true;
                n = leadingFrames(c);
                if (n == 0 || !growPending(c, n))
                    break;
                memset(c->pending, 0, (size_t)n * ch * sizeof(float));
                c->pendingStart = 0;
                c->pendingCount = n;
                continue;
            }
            n = c->resampler != NULL
                ? (UInt32)ceil((want - produced) * c->src.mSampleRate / c->dst.mSampleRate) + 1
                : want - produced;
            if (n > kChunkFrames * 4)
                n = kChunkFrames * 4;
            memset(c->inList, 0, offsetof(AudioBufferList, mBuffers) + kMaxBuffers * sizeof(AudioBuffer));
            c->inList->mNumberBuffers = (c->src.mFormatFlags & kAudioFormatFlagIsNonInterleaved)
                                        ? c->src.mChannelsPerFrame : 1;
            status = proc(c, &n, c->inList, &descriptions, userData);
            if (status != noErr)
                stop = true;    /* convert what came, then return the error */
            if (n == 0) {
                if (status == noErr)
                    c->ended = true;
                continue;
            }
            have = framesIn(&c->src, c->inList);
            if (n > have)
                n = have;
            if (n == 0)
                continue;
            if (takeInput(c, c->inList, n) != noErr) {
                status = kAudio_MemFullError;
                break;
            }
        }

        if (c->resampler != NULL) {
            spx_uint32_t inLen = c->pendingCount, outLen = want - produced;
            if (outLen > kOutFrames)
                outLen = kOutFrames;
            speex_resampler_process_interleaved_float(c->resampler, c->pending + (size_t)c->pendingStart * ch,
                                                      &inLen, c->out, &outLen);
            c->pendingStart += inLen;
            c->pendingCount -= inLen;
            _AUFromFloat(&c->dst, c->out, outLen, out, produced);
            produced += outLen;
        } else {
            UInt32 k = want - produced < c->pendingCount ? want - produced : c->pendingCount;
            _AUFromFloat(&c->dst, c->pending + (size_t)c->pendingStart * ch, k, out, produced);
            c->pendingStart += k;
            c->pendingCount -= k;
            produced += k;
        }
    }
    setOutputSizes(c, out, produced);
    *ioPackets = produced;
    return status;
}

OSStatus AudioConverterConvertComplexBuffer(AudioConverterRef c, UInt32 frames, const AudioBufferList *in,
                                            AudioBufferList *out) {
    if (!alive(c) || in == NULL || out == NULL)
        return kAudio_ParamError;
    if (resampling(c))
        return kAudioConverterErr_OperationNotSupported;
    if (framesIn(&c->src, in) < frames)
        return kAudioConverterErr_InvalidInputSize;
    if (framesIn(&c->dst, out) < frames)
        return kAudioConverterErr_InvalidOutputSize;
    /* Pending is the scratch here: a whole call is converted at once. */
    if (takeInput(c, in, frames) != noErr)
        return kAudio_MemFullError;
    _AUFromFloat(&c->dst, c->pending, frames, out, 0);
    c->pendingStart = c->pendingCount = 0;
    setOutputSizes(c, out, frames);
    return noErr;
}

OSStatus AudioConverterConvertBuffer(AudioConverterRef c, UInt32 inSize, const void *inData, UInt32 *ioOutSize,
                                     void *outData) {
    AudioBufferList in, out;
    UInt32 frames;
    OSStatus status;

    if (!alive(c) || inData == NULL || ioOutSize == NULL || outData == NULL)
        return kAudio_ParamError;
    if ((c->src.mFormatFlags | c->dst.mFormatFlags) & kAudioFormatFlagIsNonInterleaved)
        return kAudioConverterErr_FormatNotSupported;
    frames = inSize / c->src.mBytesPerFrame;
    if (*ioOutSize < frames * c->dst.mBytesPerFrame)
        return kAudioConverterErr_InvalidOutputSize;
    in.mNumberBuffers = 1;
    in.mBuffers[0].mNumberChannels = c->src.mChannelsPerFrame;
    in.mBuffers[0].mDataByteSize = inSize;
    in.mBuffers[0].mData = (void *)inData;
    out.mNumberBuffers = 1;
    out.mBuffers[0].mNumberChannels = c->dst.mChannelsPerFrame;
    out.mBuffers[0].mDataByteSize = *ioOutSize;
    out.mBuffers[0].mData = outData;
    status = AudioConverterConvertComplexBuffer(c, frames, &in, &out);
    if (status == noErr)
        *ioOutSize = out.mBuffers[0].mDataByteSize;
    return status;
}

/* The older form: the input procedure returns bytes of interleaved data. */
typedef struct {
    AudioConverterInputDataProc proc;
    void *userData;
} LegacyInput;

static OSStatus legacyInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                            AudioStreamPacketDescription **descriptions, void *userData) {
    LegacyInput *legacy = userData;
    UInt32 size = *ioPackets * c->src.mBytesPerFrame;
    void *bytes = NULL;
    OSStatus status = legacy->proc(c, &size, &bytes, legacy->userData);

    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = c->src.mChannelsPerFrame;
    data->mBuffers[0].mDataByteSize = bytes != NULL ? size : 0;
    data->mBuffers[0].mData = bytes;
    *ioPackets = bytes != NULL ? size / c->src.mBytesPerFrame : 0;
    return status;
}

OSStatus AudioConverterFillBuffer(AudioConverterRef c, AudioConverterInputDataProc proc, void *userData,
                                  UInt32 *ioOutSize, void *outData) {
    LegacyInput legacy = { proc, userData };
    AudioBufferList out;
    UInt32 frames;
    OSStatus status;

    if (!alive(c) || proc == NULL || ioOutSize == NULL || outData == NULL)
        return kAudio_ParamError;
    if ((c->src.mFormatFlags | c->dst.mFormatFlags) & kAudioFormatFlagIsNonInterleaved)
        return kAudioConverterErr_FormatNotSupported;
    frames = *ioOutSize / c->dst.mBytesPerFrame;
    out.mNumberBuffers = 1;
    out.mBuffers[0].mNumberChannels = c->dst.mChannelsPerFrame;
    out.mBuffers[0].mDataByteSize = frames * c->dst.mBytesPerFrame;
    out.mBuffers[0].mData = outData;
    status = AudioConverterFillComplexBuffer(c, legacyInput, &legacy, &frames, &out, NULL);
    *ioOutSize = frames * c->dst.mBytesPerFrame;
    return status;
}
