/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AudioConverter's decoders: a compressed source (AAC, MP3, ALAC, FLAC,
 * Opus, AC-3...) decoded to linear PCM by FFmpeg, in the app's process as
 * on the reference platform. FFmpeg's libraries (/usr/lib/media) are
 * loaded only when a converter needs them, so an app that never decodes
 * does not map them. The decoder's output goes through swresample to the
 * destination's sample format, layout, rate and channels.
 *
 * The packets come from the app's input procedure (with their
 * descriptions); the decoded frames left over from one call wait for the
 * next. The input procedure's error comes back with the frames made
 * before it, as the reference does.
 */

#include "AUInternal.h"
#include "AudioConverter.h"
#include "AudioFormat.h"
#include <CoreAudio/CoreAudio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#define kMediaLibraries "/usr/lib/media/"

/* FFmpeg's functions used here, found when first needed. */
static struct {
    const AVCodec *(*find_decoder)(enum AVCodecID);
    AVCodecContext *(*alloc_context3)(const AVCodec *);
    int (*open2)(AVCodecContext *, const AVCodec *, AVDictionary **);
    void (*free_context)(AVCodecContext **);
    int (*send_packet)(AVCodecContext *, const AVPacket *);
    int (*receive_frame)(AVCodecContext *, AVFrame *);
    void (*flush_buffers)(AVCodecContext *);
    AVPacket *(*packet_alloc)(void);
    void (*packet_free)(AVPacket **);
    int (*new_packet)(AVPacket *, int);
    void (*packet_unref)(AVPacket *);
    AVFrame *(*frame_alloc)(void);
    void (*frame_free)(AVFrame **);
    void (*frame_unref)(AVFrame *);
    void *(*mallocz)(size_t);
    void (*layout_default)(AVChannelLayout *, int);
    void (*layout_uninit)(AVChannelLayout *);
    int (*layout_copy)(AVChannelLayout *, const AVChannelLayout *);
    enum AVChannel (*layout_channel)(const AVChannelLayout *, unsigned int);
    int (*swr_alloc_set_opts2)(struct SwrContext **, const AVChannelLayout *, enum AVSampleFormat, int,
                               const AVChannelLayout *, enum AVSampleFormat, int, int, void *);
    int (*swr_init)(struct SwrContext *);
    void (*swr_free)(struct SwrContext **);
    int (*swr_convert)(struct SwrContext *, uint8_t *const *, int, const uint8_t *const *, int);
    int (*swr_get_out_samples)(struct SwrContext *, int);
} ff;
static int ffLoaded;            /* 1 loaded, -1 not there */
static pthread_once_t ffOnce = PTHREAD_ONCE_INIT;

static void loadFFmpeg(void) {
    void *avcodec = dlopen(kMediaLibraries "libavcodec.dylib", RTLD_LAZY | RTLD_LOCAL);
    void *avutil = dlopen(kMediaLibraries "libavutil.dylib", RTLD_LAZY | RTLD_LOCAL);
    void *swr = dlopen(kMediaLibraries "libswresample.dylib", RTLD_LAZY | RTLD_LOCAL);

    ffLoaded = -1;
    if (avcodec == NULL || avutil == NULL || swr == NULL)
        return;
#define FIND(lib, field, name)                       \
    if ((*(void **)&ff.field = dlsym(lib, name)) == NULL) \
        return;
    FIND(avcodec, find_decoder, "avcodec_find_decoder")
    FIND(avcodec, alloc_context3, "avcodec_alloc_context3")
    FIND(avcodec, open2, "avcodec_open2")
    FIND(avcodec, free_context, "avcodec_free_context")
    FIND(avcodec, send_packet, "avcodec_send_packet")
    FIND(avcodec, receive_frame, "avcodec_receive_frame")
    FIND(avcodec, flush_buffers, "avcodec_flush_buffers")
    FIND(avcodec, packet_alloc, "av_packet_alloc")
    FIND(avcodec, packet_free, "av_packet_free")
    FIND(avcodec, new_packet, "av_new_packet")
    FIND(avcodec, packet_unref, "av_packet_unref")
    FIND(avutil, frame_alloc, "av_frame_alloc")
    FIND(avutil, frame_free, "av_frame_free")
    FIND(avutil, frame_unref, "av_frame_unref")
    FIND(avutil, mallocz, "av_mallocz")
    FIND(avutil, layout_default, "av_channel_layout_default")
    FIND(avutil, layout_uninit, "av_channel_layout_uninit")
    FIND(avutil, layout_copy, "av_channel_layout_copy")
    FIND(avutil, layout_channel, "av_channel_layout_channel_from_index")
    FIND(swr, swr_alloc_set_opts2, "swr_alloc_set_opts2")
    FIND(swr, swr_init, "swr_init")
    FIND(swr, swr_free, "swr_free")
    FIND(swr, swr_convert, "swr_convert")
    FIND(swr, swr_get_out_samples, "swr_get_out_samples")
#undef FIND
    ffLoaded = 1;
}

static Boolean haveFFmpeg(void) {
    pthread_once(&ffOnce, loadFFmpeg);
    return ffLoaded == 1;
}

/* The decoder for a format, or AV_CODEC_ID_NONE. */
static enum AVCodecID codecFor(AudioFormatID f) {
    switch (f) {
    case kAudioFormatMPEG4AAC:
    case kAudioFormatMPEG4AAC_HE:
    case kAudioFormatMPEG4AAC_HE_V2:
    case kAudioFormatMPEG4AAC_LD:
    case kAudioFormatMPEG4AAC_ELD:
        return AV_CODEC_ID_AAC;
    case kAudioFormatMPEGLayer1:
        return AV_CODEC_ID_MP1;
    case kAudioFormatMPEGLayer2:
        return AV_CODEC_ID_MP2;
    case kAudioFormatMPEGLayer3:
        return AV_CODEC_ID_MP3;
    case kAudioFormatAppleLossless:
        return AV_CODEC_ID_ALAC;
    case kAudioFormatFLAC:
        return AV_CODEC_ID_FLAC;
    case kAudioFormatOpus:
        return AV_CODEC_ID_OPUS;
    case kAudioFormatAC3:
        return AV_CODEC_ID_AC3;
    case kAudioFormatEnhancedAC3:
        return AV_CODEC_ID_EAC3;
    case kAudioFormatULaw:
        return AV_CODEC_ID_PCM_MULAW;
    case kAudioFormatALaw:
        return AV_CODEC_ID_PCM_ALAW;
    case kAudioFormatAMR:
        return AV_CODEC_ID_AMR_NB;
    case kAudioFormatAMR_WB:
        return AV_CODEC_ID_AMR_WB;
    default:
        return AV_CODEC_ID_NONE;
    }
}

Boolean _ATCodecCanDecode(AudioFormatID f) {
    return codecFor(f) != AV_CODEC_ID_NONE && haveFFmpeg() && ff.find_decoder(codecFor(f)) != NULL;
}

/* The destination's sample format for swresample (packed or planar). */
static enum AVSampleFormat sampleFormatOf(const AudioStreamBasicDescription *f) {
    Boolean planar = (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;

    if (f->mFormatID != kAudioFormatLinearPCM || (f->mFormatFlags & kAudioFormatFlagIsBigEndian))
        return AV_SAMPLE_FMT_NONE;
    if (f->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (f->mBitsPerChannel == 32)
            return planar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
        if (f->mBitsPerChannel == 64)
            return planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
        return AV_SAMPLE_FMT_NONE;
    }
    if (f->mFormatFlags & kAudioFormatFlagIsSignedInteger) {
        if (f->mBitsPerChannel == 16 && f->mBytesPerFrame == 2 * (planar ? 1 : f->mChannelsPerFrame))
            return planar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
        if (f->mBitsPerChannel == 32 && f->mBytesPerFrame == 4 * (planar ? 1 : f->mChannelsPerFrame))
            return planar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
        return AV_SAMPLE_FMT_NONE;
    }
    if (f->mBitsPerChannel == 8 && f->mBytesPerFrame == (planar ? 1 : f->mChannelsPerFrame))
        return planar ? AV_SAMPLE_FMT_U8P : AV_SAMPLE_FMT_U8;
    return AV_SAMPLE_FMT_NONE;
}

struct ATCodec {
    AudioStreamBasicDescription src, dst;
    enum AVCodecID codecID;
    AVCodecContext *ctx;
    struct SwrContext *swr;
    AVChannelLayout swrInLayout;
    int swrInRate;
    enum AVSampleFormat swrInFormat;
    AVPacket *packet;
    AVFrame *frame;
    UInt8 *cookie;
    UInt32 cookieSize;
    Boolean opened, drained;

    /* Converted frames waiting for the app, one plane per buffer. */
    UInt8 *left[kMaxChannels];
    UInt32 leftStart, leftCount, leftCapacity;
    UInt32 planes, bytesPerFrame;   /* of the destination, per plane */
};

ATCodec *_ATCodecNew(const AudioStreamBasicDescription *src, const AudioStreamBasicDescription *dst, OSStatus *status) {
    ATCodec *c;

    *status = kAudioConverterErr_FormatNotSupported;
    if (codecFor(src->mFormatID) == AV_CODEC_ID_NONE || sampleFormatOf(dst) == AV_SAMPLE_FMT_NONE ||
        dst->mChannelsPerFrame == 0 || dst->mChannelsPerFrame > kMaxChannels || dst->mSampleRate <= 0)
        return NULL;
    if (!haveFFmpeg() || ff.find_decoder(codecFor(src->mFormatID)) == NULL)
        return NULL;
    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        *status = kAudio_MemFullError;
        return NULL;
    }
    c->src = *src;
    c->dst = *dst;
    c->codecID = codecFor(src->mFormatID);
    c->planes = (dst->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? dst->mChannelsPerFrame : 1;
    c->bytesPerFrame = dst->mBytesPerFrame;
    c->packet = ff.packet_alloc();
    c->frame = ff.frame_alloc();
    if (c->packet == NULL || c->frame == NULL) {
        _ATCodecDispose(c);
        *status = kAudio_MemFullError;
        return NULL;
    }
    *status = noErr;
    return c;
}

static void closeDecoder(ATCodec *c) {
    if (c->ctx != NULL)
        ff.free_context(&c->ctx);
    if (c->swr != NULL)
        ff.swr_free(&c->swr);
    ff.layout_uninit(&c->swrInLayout);
    c->opened = false;
}

void _ATCodecDispose(ATCodec *c) {
    if (c == NULL)
        return;
    closeDecoder(c);
    if (c->packet != NULL)
        ff.packet_free(&c->packet);
    if (c->frame != NULL)
        ff.frame_free(&c->frame);
    for (UInt32 i = 0; i < kMaxChannels; i++)
        free(c->left[i]);
    free(c->cookie);
    free(c);
}

/* The AudioSpecificConfig out of an AAC cookie (an ES descriptor, with or
   without the esds box's version and flags, or the config itself). */
static const UInt8 *extradataOf(ATCodec *c, UInt32 *size) {
    *size = c->cookieSize;
    if (c->cookie == NULL)
        return NULL;
    if (c->codecID == AV_CODEC_ID_AAC)
        return _ATFindSpecificConfig(c->cookie, c->cookieSize, size);
    return c->cookie;
}

static OSStatus openDecoder(ATCodec *c) {
    const AVCodec *codec = ff.find_decoder(c->codecID);
    const UInt8 *extra;
    UInt32 extraSize;

    if (codec == NULL)
        return kAudioConverterErr_FormatNotSupported;
    c->ctx = ff.alloc_context3(codec);
    if (c->ctx == NULL)
        return kAudio_MemFullError;
    c->ctx->sample_rate = (int)c->src.mSampleRate;
    if (c->src.mChannelsPerFrame > 0)
        ff.layout_default(&c->ctx->ch_layout, (int)c->src.mChannelsPerFrame);
    extra = extradataOf(c, &extraSize);
    if (extra != NULL && extraSize > 0) {
        c->ctx->extradata = ff.mallocz(extraSize + AV_INPUT_BUFFER_PADDING_SIZE);
        if (c->ctx->extradata == NULL)
            return kAudio_MemFullError;
        memcpy(c->ctx->extradata, extra, extraSize);
        c->ctx->extradata_size = (int)extraSize;
    }
    if (ff.open2(c->ctx, codec, NULL) < 0) {
        ff.free_context(&c->ctx);
        return kAudioConverterErr_FormatNotSupported;
    }
    c->opened = true;
    c->drained = false;
    return noErr;
}

OSStatus _ATCodecSetCookie(ATCodec *c, const void *data, UInt32 size) {
    UInt8 *copy = size > 0 ? malloc(size) : NULL;

    if (size > 0 && copy == NULL)
        return kAudio_MemFullError;
    if (size > 0)
        memcpy(copy, data, size);
    free(c->cookie);
    c->cookie = copy;
    c->cookieSize = size;
    closeDecoder(c);            /* opened again with it */
    return noErr;
}

const void *_ATCodecCookie(ATCodec *c, UInt32 *size) {
    *size = c->cookieSize;
    return c->cookie;
}

void _ATCodecReset(ATCodec *c) {
    if (c->ctx != NULL)
        ff.flush_buffers(c->ctx);
    c->drained = false;
    c->leftStart = c->leftCount = 0;
    if (c->swr != NULL)
        ff.swr_free(&c->swr);   /* its delay is old samples */
}

UInt32 _ATCodecChannels(ATCodec *c) {
    return c->dst.mChannelsPerFrame;
}

/* The label of a destination channel: FFmpeg's default order for that
   many channels, as swresample gives it. */
AudioChannelLabel _ATCodecChannelLabel(ATCodec *c, UInt32 index) {
    AVChannelLayout l;
    enum AVChannel ch;

    if (c->dst.mChannelsPerFrame == 1)
        return kAudioChannelLabel_Mono;
    if (!haveFFmpeg())
        return kAudioChannelLabel_Discrete_0 + index;
    memset(&l, 0, sizeof(l));
    ff.layout_default(&l, (int)c->dst.mChannelsPerFrame);
    ch = ff.layout_channel(&l, index);
    ff.layout_uninit(&l);
    switch (ch) {
    case AV_CHAN_FRONT_LEFT: return kAudioChannelLabel_Left;
    case AV_CHAN_FRONT_RIGHT: return kAudioChannelLabel_Right;
    case AV_CHAN_FRONT_CENTER: return kAudioChannelLabel_Center;
    case AV_CHAN_LOW_FREQUENCY: return kAudioChannelLabel_LFEScreen;
    case AV_CHAN_BACK_LEFT: return kAudioChannelLabel_RearSurroundLeft;
    case AV_CHAN_BACK_RIGHT: return kAudioChannelLabel_RearSurroundRight;
    case AV_CHAN_BACK_CENTER: return kAudioChannelLabel_CenterSurround;
    case AV_CHAN_SIDE_LEFT: return kAudioChannelLabel_LeftSurround;
    case AV_CHAN_SIDE_RIGHT: return kAudioChannelLabel_RightSurround;
    default: return kAudioChannelLabel_Discrete_0 + index;
    }
}

static Boolean growLeft(ATCodec *c, UInt32 frames) {
    UInt32 need = c->leftCount + frames;

    if (c->leftStart > 0 && c->leftCount > 0)
        for (UInt32 p = 0; p < c->planes; p++)
            memmove(c->left[p], c->left[p] + c->leftStart * c->bytesPerFrame, c->leftCount * c->bytesPerFrame);
    c->leftStart = 0;
    if (need <= c->leftCapacity)
        return true;
    for (UInt32 p = 0; p < c->planes; p++) {
        UInt8 *grown = realloc(c->left[p], (size_t)need * c->bytesPerFrame);
        if (grown == NULL)
            return false;
        c->left[p] = grown;
    }
    c->leftCapacity = need;
    return true;
}

/* A decoded frame converted to the destination, appended to the left over. */
static OSStatus convertFrame(ATCodec *c) {
    AVFrame *f = c->frame;
    enum AVSampleFormat out = sampleFormatOf(&c->dst);
    int outRate = (int)c->dst.mSampleRate, made;
    UInt8 *planes[kMaxChannels];

    /* A new swresample context when the decoder's output changes. */
    if (c->swr == NULL || c->swrInRate != f->sample_rate || c->swrInFormat != f->format ||
        c->swrInLayout.nb_channels != f->ch_layout.nb_channels) {
        AVChannelLayout outLayout;
        if (c->swr != NULL)
            ff.swr_free(&c->swr);
        ff.layout_uninit(&c->swrInLayout);
        ff.layout_copy(&c->swrInLayout, &f->ch_layout);
        c->swrInRate = f->sample_rate;
        c->swrInFormat = f->format;
        memset(&outLayout, 0, sizeof(outLayout));
        ff.layout_default(&outLayout, (int)c->dst.mChannelsPerFrame);
        if (ff.swr_alloc_set_opts2(&c->swr, &outLayout, out, outRate, &f->ch_layout, f->format, f->sample_rate, 0,
                                   NULL) < 0 ||
            ff.swr_init(c->swr) < 0) {
            ff.layout_uninit(&outLayout);
            if (c->swr != NULL)
                ff.swr_free(&c->swr);
            return kAudioConverterErr_UnspecifiedError;
        }
        ff.layout_uninit(&outLayout);
    }
    int room = ff.swr_get_out_samples(c->swr, f->nb_samples);
    if (room < 0 || !growLeft(c, (UInt32)room))
        return kAudio_MemFullError;
    for (UInt32 p = 0; p < c->planes; p++)
        planes[p] = c->left[p] + (c->leftStart + c->leftCount) * c->bytesPerFrame;
    made = ff.swr_convert(c->swr, planes, room, (const uint8_t *const *)f->extended_data, f->nb_samples);
    if (made < 0)
        return kAudioConverterErr_UnspecifiedError;
    c->leftCount += (UInt32)made;
    return noErr;
}

/* Packets from the app to the decoder: true when some were sent. */
static Boolean feed(ATCodec *c, AudioConverterRef converter, AudioConverterComplexInputDataProc proc,
                    void *userData, AudioBufferList *list, OSStatus *inputError) {
    UInt32 packets = 1;
    AudioStreamPacketDescription *descriptions = NULL;
    OSStatus err;

    list->mNumberBuffers = 1;
    list->mBuffers[0].mData = NULL;
    list->mBuffers[0].mDataByteSize = 0;
    list->mBuffers[0].mNumberChannels = c->src.mChannelsPerFrame;
    err = proc(converter, &packets, list, &descriptions, userData);
    if (err != noErr || packets == 0) {
        *inputError = err;
        return false;
    }
    const UInt8 *bytes = list->mBuffers[0].mData;
    UInt32 total = list->mBuffers[0].mDataByteSize;
    for (UInt32 i = 0; i < packets; i++) {
        UInt32 start = 0, size = total;
        if (descriptions != NULL) {
            start = (UInt32)descriptions[i].mStartOffset;
            size = descriptions[i].mDataByteSize;
        } else if (c->src.mBytesPerPacket > 0) {
            start = i * c->src.mBytesPerPacket;
            size = c->src.mBytesPerPacket;
        } else if (i > 0) {
            break;      /* one packet, the whole buffer */
        }
        if (bytes == NULL || size == 0 || start + size > total)
            continue;
        if (ff.new_packet(c->packet, (int)size) < 0)
            continue;
        memcpy(c->packet->data, bytes + start, size);
        ff.send_packet(c->ctx, c->packet);
        ff.packet_unref(c->packet);
    }
    return true;
}

OSStatus _ATCodecFill(ATCodec *c, AudioConverterRef converter, AudioConverterComplexInputDataProc proc,
                      void *userData, UInt32 *ioFrames, AudioBufferList *out, AudioBufferList *scratch) {
    UInt32 wanted = *ioFrames, made = 0;
    OSStatus inputError = noErr, status;

    if (!c->opened && (status = openDecoder(c)) != noErr) {
        *ioFrames = 0;
        return status;
    }
    if (out == NULL || out->mNumberBuffers < c->planes) {
        *ioFrames = 0;
        return kAudio_ParamError;
    }
    for (UInt32 p = 0; p < c->planes; p++)
        if (out->mBuffers[p].mData == NULL || out->mBuffers[p].mDataByteSize < c->bytesPerFrame) {
            *ioFrames = 0;
            return kAudio_ParamError;
        }
    for (UInt32 p = 0; p < c->planes; p++) {
        UInt32 fit = out->mBuffers[p].mDataByteSize / c->bytesPerFrame;
        if (fit < wanted)
            wanted = fit;
    }

    while (made < wanted) {
        if (c->leftCount > 0) {
            UInt32 n = c->leftCount < wanted - made ? c->leftCount : wanted - made;
            for (UInt32 p = 0; p < c->planes; p++)
                memcpy((UInt8 *)out->mBuffers[p].mData + made * c->bytesPerFrame,
                       c->left[p] + c->leftStart * c->bytesPerFrame, n * c->bytesPerFrame);
            c->leftStart += n;
            c->leftCount -= n;
            made += n;
            continue;
        }
        int r = ff.receive_frame(c->ctx, c->frame);
        if (r == 0) {
            status = convertFrame(c);
            ff.frame_unref(c->frame);
            if (status != noErr)
                break;
            continue;
        }
        if (r == AVERROR_EOF)
            break;
        if (r != AVERROR(EAGAIN))
            break;      /* a broken packet: skip it, as players do */
        if (c->drained)
            break;
        if (!feed(c, converter, proc, userData, scratch, &inputError)) {
            if (inputError != noErr)
                break;
            /* The end of the input: what the decoder still holds. */
            ff.send_packet(c->ctx, NULL);
            c->drained = true;
        }
    }
    for (UInt32 p = 0; p < c->planes; p++)
        out->mBuffers[p].mDataByteSize = made * c->bytesPerFrame;
    *ioFrames = made;
    return inputError;
}
