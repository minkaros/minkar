/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Linear PCM to and from interleaved float, and from one channel count to
 * another: what the units and converters work in.
 */

#include "AUInternal.h"
#include <math.h>
#include <string.h>

#define kMaxChannels 32

Boolean _AUFormatIsSupported(const AudioStreamBasicDescription *f) {
    UInt32 bytes;

    if (f == NULL || f->mFormatID != kAudioFormatLinearPCM || f->mSampleRate <= 0 ||
        f->mFramesPerPacket != 1 || f->mChannelsPerFrame == 0 || f->mChannelsPerFrame > kMaxChannels)
        return false;
    bytes = f->mBitsPerChannel / 8;
    if (f->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (f->mBitsPerChannel != 32 && f->mBitsPerChannel != 64)
            return false;
    } else if (f->mFormatFlags & kAudioFormatFlagIsSignedInteger) {
        if (f->mBitsPerChannel != 8 && f->mBitsPerChannel != 16 && f->mBitsPerChannel != 24 &&
            f->mBitsPerChannel != 32)
            return false;
    } else if (f->mBitsPerChannel != 8) {   /* unsigned 8-bit */
        return false;
    }
    /* Packed samples, one per channel slot. */
    if (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
        return f->mBytesPerFrame == bytes;
    return f->mBytesPerFrame == bytes * f->mChannelsPerFrame;
}

/* A sample's bytes, least significant first. */
static inline void load(const AudioStreamBasicDescription *f, const unsigned char *p, unsigned char *le, UInt32 n) {
    if (f->mFormatFlags & kAudioFormatFlagIsBigEndian)
        for (UInt32 i = 0; i < n; i++)
            le[i] = p[n - 1 - i];
    else
        memcpy(le, p, n);
}

static inline void store(const AudioStreamBasicDescription *f, unsigned char *p, const unsigned char *le, UInt32 n) {
    if (f->mFormatFlags & kAudioFormatFlagIsBigEndian)
        for (UInt32 i = 0; i < n; i++)
            p[i] = le[n - 1 - i];
    else
        memcpy(p, le, n);
}

static inline float readSample(const AudioStreamBasicDescription *f, const unsigned char *p) {
    unsigned char b[8];
    UInt32 n = f->mBitsPerChannel / 8;

    load(f, p, b, n);
    if (f->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (n == 4) {
            float v;
            memcpy(&v, b, sizeof(v));
            return v;
        } else {
            double v;
            memcpy(&v, b, sizeof(v));
            return (float)v;
        }
    }
    if (f->mFormatFlags & kAudioFormatFlagIsSignedInteger) {
        switch (n) {
        case 1:
            return (int8_t)b[0] * (1.0f / 128.0f);
        case 2:
            return (int16_t)(b[0] | b[1] << 8) * (1.0f / 32768.0f);
        case 3:
            return ((int32_t)((uint32_t)b[0] << 8 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 24) >> 8) *
                   (1.0f / 8388608.0f);
        default:
            return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24) *
                   (1.0f / 2147483648.0f);
        }
    }
    return (b[0] - 128) * (1.0f / 128.0f);
}

/* Mono goes to every output channel; extra input channels are dropped and
   missing ones are silent. */
void _AUToFloat(const AudioStreamBasicDescription *f, const AudioBufferList *in, UInt32 frames,
                float *out, UInt32 outChannels) {
    UInt32 inChannels = f->mChannelsPerFrame;
    UInt32 bytes = f->mBitsPerChannel / 8;
    Boolean planar = (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;

    for (UInt32 c = 0; c < outChannels; c++) {
        UInt32 src = inChannels == 1 ? 0 : c;
        const unsigned char *base;
        UInt32 stride, offset, available;

        if (src >= inChannels) {
            for (UInt32 i = 0; i < frames; i++)
                out[i * outChannels + c] = 0;
            continue;
        }
        if (planar) {
            if (src >= in->mNumberBuffers || in->mBuffers[src].mData == NULL) {
                for (UInt32 i = 0; i < frames; i++)
                    out[i * outChannels + c] = 0;
                continue;
            }
            base = in->mBuffers[src].mData;
            stride = bytes;
            offset = 0;
            available = in->mBuffers[src].mDataByteSize / bytes;
        } else {
            base = in->mNumberBuffers > 0 ? in->mBuffers[0].mData : NULL;
            stride = f->mBytesPerFrame;
            offset = src * bytes;
            available = base != NULL ? in->mBuffers[0].mDataByteSize / stride : 0;
        }
        for (UInt32 i = 0; i < frames; i++)
            out[i * outChannels + c] = i < available ? readSample(f, base + i * stride + offset) : 0;
    }
}

static inline void writeSample(const AudioStreamBasicDescription *f, unsigned char *p, float v) {
    unsigned char b[8];
    UInt32 n = f->mBitsPerChannel / 8;

    if (f->mFormatFlags & kAudioFormatFlagIsFloat) {
        if (n == 4) {
            memcpy(b, &v, sizeof(v));
        } else {
            double d = v;
            memcpy(b, &d, sizeof(d));
        }
        store(f, p, b, n);
        return;
    }
    if (v > 1)
        v = 1;
    else if (v < -1)
        v = -1;
    if (f->mFormatFlags & kAudioFormatFlagIsSignedInteger) {
        int32_t s;
        switch (n) {
        case 1:
            s = (int32_t)lrintf(v * 127.0f);
            break;
        case 2:
            s = (int32_t)lrintf(v * 32767.0f);
            break;
        case 3:
            s = (int32_t)lrintf(v * 8388607.0f);
            break;
        default:
            s = (int32_t)lrint((double)v * 2147483647.0);
            break;
        }
        for (UInt32 i = 0; i < n; i++)
            b[i] = (unsigned char)((uint32_t)s >> (8 * i));
        store(f, p, b, n);
        return;
    }
    p[0] = (unsigned char)lrintf(v * 127.0f + 128.0f);
}

void _AUFromFloat(const AudioStreamBasicDescription *f, const float *in, UInt32 frames, AudioBufferList *out,
                  UInt32 offset) {
    UInt32 channels = f->mChannelsPerFrame;
    UInt32 bytes = f->mBitsPerChannel / 8;
    Boolean planar = (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;

    for (UInt32 c = 0; c < channels; c++) {
        unsigned char *base;
        UInt32 stride, at;

        if (planar) {
            if (c >= out->mNumberBuffers || out->mBuffers[c].mData == NULL)
                continue;
            base = out->mBuffers[c].mData;
            stride = bytes;
            at = 0;
        } else {
            if (out->mNumberBuffers == 0 || out->mBuffers[0].mData == NULL)
                return;
            base = out->mBuffers[0].mData;
            stride = f->mBytesPerFrame;
            at = c * bytes;
        }
        for (UInt32 i = 0; i < frames; i++)
            writeSample(f, base + (offset + i) * stride + at, in[i * channels + c]);
    }
}

/* Channels from one count to another: a map (output channel -> input
   channel, or -1 for silence) when given; otherwise the same channels,
   mono to every channel, two or more to mono as their mean of the first
   two, and the rest dropped or silent. */
void _AUMapChannels(const float *in, UInt32 inChannels, float *out, UInt32 outChannels, UInt32 frames,
                    const SInt32 *map) {
    for (UInt32 i = 0; i < frames; i++) {
        const float *src = in + i * inChannels;
        float *dst = out + i * outChannels;

        for (UInt32 c = 0; c < outChannels; c++) {
            if (map != NULL)
                dst[c] = map[c] >= 0 && (UInt32)map[c] < inChannels ? src[map[c]] : 0;
            else if (inChannels == 1)
                dst[c] = src[0];
            else if (outChannels == 1)
                dst[c] = (src[0] + src[1]) * 0.5f;
            else
                dst[c] = c < inChannels ? src[c] : 0;
        }
    }
}
