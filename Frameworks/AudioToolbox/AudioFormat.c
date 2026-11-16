/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio format information: a format's fields filled in (linear PCM, and
 * the compressed formats' packet sizes, with AAC's rate and channels from
 * its decoder configuration), names, and channel layouts (by tag, by
 * bitmap, their channel counts and names). Converters for compressed
 * formats come with the codecs (AU4).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioFormat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Formats --------------------------------------------------------------- */

typedef struct {
    UInt32 id;
    const char *name;
    UInt32 framesPerPacket;     /* 0: variable or unknown */
    Boolean vbr, externallyFramed;
} FormatEntry;

static const FormatEntry formats[] = {
    { kAudioFormatLinearPCM, "Linear PCM", 1, false, false },
    { kAudioFormatMPEG4AAC, "AAC", 1024, true, true },
    { kAudioFormatMPEG4AAC_HE, "HE-AAC", 2048, true, true },
    { kAudioFormatMPEG4AAC_HE_V2, "HE-AAC v2", 2048, true, true },
    { kAudioFormatMPEG4AAC_LD, "AAC Low Delay", 512, true, true },
    { kAudioFormatMPEG4AAC_ELD, "AAC Enhanced Low Delay", 512, true, true },
    { kAudioFormatMPEGLayer1, "MPEG Layer 1", 384, true, false },
    { kAudioFormatMPEGLayer2, "MPEG Layer 2", 1152, true, false },
    { kAudioFormatMPEGLayer3, "MPEG Layer 3", 1152, true, false },
    { kAudioFormatAC3, "AC-3", 1536, false, false },
    { kAudioFormatEnhancedAC3, "Enhanced AC-3", 1536, true, false },
    { kAudioFormatAppleLossless, "Apple Lossless", 4096, true, true },
    { kAudioFormatFLAC, "FLAC", 0, true, true },
    { kAudioFormatOpus, "Opus", 960, true, true },
    { kAudioFormatULaw, "u-Law", 1, false, false },
    { kAudioFormatALaw, "a-Law", 1, false, false },
    { kAudioFormatAppleIMA4, "IMA 4:1", 64, false, false },
    { kAudioFormatAMR, "AMR Narrowband", 160, true, false },
    { kAudioFormatAMR_WB, "AMR Wideband", 320, true, false },
    { kAudioFormatiLBC, "iLBC", 0, false, false },
};
#define kFormatCount (sizeof(formats) / sizeof(formats[0]))

static const FormatEntry *formatEntry(UInt32 id) {
    for (size_t i = 0; i < kFormatCount; i++)
        if (formats[i].id == id)
            return &formats[i];
    return NULL;
}

/* MPEG-4 audio's AudioSpecificConfig: its object type, rate and channels.
   The configuration is either the cookie itself or inside an ES
   descriptor (tag 5, decoder-specific information). */
static Boolean descriptorLength(const UInt8 **p, const UInt8 *end, UInt32 *len) {
    *len = 0;
    for (int i = 0; i < 4 && *p < end; i++) {
        UInt8 b = *(*p)++;
        *len = *len << 7 | (b & 0x7f);
        if (!(b & 0x80))
            return true;
    }
    return false;
}

static const UInt8 *findSpecificConfig(const UInt8 *p, UInt32 size, UInt32 *outSize) {
    const UInt8 *end = p + size;

    /* An ES descriptor starts with tag 3 (perhaps after a version word). */
    for (int skip = 0; skip <= 4 && skip < (int)size; skip += 4) {
        const UInt8 *q = p + skip;
        UInt32 len;
        if (*q != 3)
            continue;
        q++;
        if (!descriptorLength(&q, end, &len) || q + 3 > end)
            continue;
        {
            UInt8 flags = q[2];
            q += 3;
            if (flags & 0x80)
                q += 2;
            if ((flags & 0x40) && q < end)
                q += 1 + *q;
            if (flags & 0x20)
                q += 2;
        }
        if (q >= end || *q != 4)
            continue;
        q++;
        if (!descriptorLength(&q, end, &len))
            continue;
        q += 13;    /* object type, stream type, buffer size, bit rates */
        if (q >= end || *q != 5)
            continue;
        q++;
        if (!descriptorLength(&q, end, &len) || q + len > end)
            continue;
        *outSize = len;
        return q;
    }
    *outSize = size;
    return p;
}

const UInt8 *_ATFindSpecificConfig(const UInt8 *cookie, UInt32 size, UInt32 *outSize) {
    return findSpecificConfig(cookie, size, outSize);
}

static Boolean parseSpecificConfig(const UInt8 *p, UInt32 size, UInt32 *objectType, Float64 *rate,
                                   UInt32 *channels) {
    static const UInt32 rates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                    16000, 12000, 11025, 8000, 7350 };
    UInt64 bits = 0;
    int have = 0, at = 0;
    UInt32 index;

#define TAKE(n, out)                                         \
    do {                                                     \
        while (have < (n)) {                                 \
            if (at >= (int)size)                             \
                return false;                                \
            bits = bits << 8 | p[at++];                      \
            have += 8;                                       \
        }                                                    \
        (out) = (UInt32)(bits >> (have - (n))) & ((1u << (n)) - 1); \
        have -= (n);                                         \
    } while (0)

    TAKE(5, *objectType);
    if (*objectType == 31) {
        UInt32 ext;
        TAKE(6, ext);
        *objectType = 32 + ext;
    }
    TAKE(4, index);
    if (index == 15) {
        UInt32 hi, lo;
        TAKE(12, hi);
        TAKE(12, lo);
        *rate = hi << 12 | lo;
    } else if (index < 13) {
        *rate = rates[index];
    } else {
        return false;
    }
    TAKE(4, *channels);
    if (*channels == 7)
        *channels = 8;
#undef TAKE
    return true;
}

static OSStatus formatInfo(AudioStreamBasicDescription *d, const void *cookie, UInt32 cookieSize) {
    const FormatEntry *e = formatEntry(d->mFormatID);

    if (e == NULL)
        return kAudioFormatUnknownFormatError;
    if (d->mFormatID == kAudioFormatLinearPCM) {
        if (d->mBitsPerChannel == 0 || d->mChannelsPerFrame == 0)
            return kAudioFormatUnsupportedDataFormatError;
        d->mFramesPerPacket = 1;
        d->mBytesPerFrame = (d->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
                            ? (d->mBitsPerChannel + 7) / 8
                            : (d->mBitsPerChannel + 7) / 8 * d->mChannelsPerFrame;
        d->mBytesPerPacket = d->mBytesPerFrame;
        return noErr;
    }
    if (cookie != NULL && cookieSize > 0 &&
        (d->mFormatID == kAudioFormatMPEG4AAC || d->mFormatID == kAudioFormatMPEG4AAC_HE ||
         d->mFormatID == kAudioFormatMPEG4AAC_HE_V2 || d->mFormatID == kAudioFormatMPEG4AAC_LD ||
         d->mFormatID == kAudioFormatMPEG4AAC_ELD)) {
        UInt32 size, objectType = 0, channels = 0;
        Float64 rate = 0;
        const UInt8 *asc = findSpecificConfig(cookie, cookieSize, &size);
        if (parseSpecificConfig(asc, size, &objectType, &rate, &channels)) {
            d->mSampleRate = rate;
            if (channels != 0)
                d->mChannelsPerFrame = channels;
            d->mFormatFlags = objectType;   /* AAC's flags are its object type */
            if (objectType == 23 || objectType == 39)
                d->mFramesPerPacket = 512;
        }
    }
    if (d->mFramesPerPacket == 0)
        d->mFramesPerPacket = e->framesPerPacket;
    if (d->mFormatID == kAudioFormatULaw || d->mFormatID == kAudioFormatALaw) {
        d->mBytesPerFrame = d->mBytesPerPacket = d->mChannelsPerFrame;
        d->mBitsPerChannel = 8;
    } else if (d->mFormatID == kAudioFormatAppleIMA4) {
        d->mBytesPerPacket = 34 * d->mChannelsPerFrame;
    } else if (e->vbr) {
        d->mBytesPerPacket = 0;
    }
    return noErr;
}

static CFStringRef formatName(const AudioStreamBasicDescription *d) {
    const FormatEntry *e = formatEntry(d->mFormatID);
    char name[160];

    if (e == NULL)
        return NULL;
    if (d->mFormatID == kAudioFormatLinearPCM) {
        const char *kind = (d->mFormatFlags & kAudioFormatFlagIsFloat) ? "floating point"
                           : (d->mFormatFlags & kAudioFormatFlagIsSignedInteger) ? "signed integer"
                                                                                  : "unsigned integer";
        snprintf(name, sizeof(name), "%s, %u-bit %s-endian %s%s", e->name, (unsigned)d->mBitsPerChannel,
                 (d->mFormatFlags & kAudioFormatFlagIsBigEndian) ? "big" : "little", kind,
                 (d->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? ", deinterleaved" : "");
    } else {
        snprintf(name, sizeof(name), "%s", e->name);
    }
    return CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
}

/* ---- Channel layouts --------------------------------------------------------- */

enum { L = kAudioChannelLabel_Left, R = kAudioChannelLabel_Right, C = kAudioChannelLabel_Center,
       LFE = kAudioChannelLabel_LFEScreen, Ls = kAudioChannelLabel_LeftSurround,
       Rs = kAudioChannelLabel_RightSurround, Lc = kAudioChannelLabel_LeftCenter,
       Rc = kAudioChannelLabel_RightCenter, Cs = kAudioChannelLabel_CenterSurround,
       Rls = kAudioChannelLabel_RearSurroundLeft, Rrs = kAudioChannelLabel_RearSurroundRight,
       Lw = kAudioChannelLabel_LeftWide, Rw = kAudioChannelLabel_RightWide };

typedef struct {
    AudioChannelLayoutTag tag;
    const char *name;
    AudioChannelLabel labels[8];
} LayoutEntry;

static const LayoutEntry layouts[] = {
    { kAudioChannelLayoutTag_Mono, "Mono", { kAudioChannelLabel_Mono } },
    { kAudioChannelLayoutTag_Stereo, "Stereo (L R)", { L, R } },
    { kAudioChannelLayoutTag_StereoHeadphones, "Stereo Headphones",
      { kAudioChannelLabel_Headphones_Left, kAudioChannelLabel_Headphones_Right } },
    { kAudioChannelLayoutTag_MatrixStereo, "Matrix Stereo", { kAudioChannelLabel_LeftTotal, kAudioChannelLabel_RightTotal } },
    { kAudioChannelLayoutTag_Quadraphonic, "Quadraphonic", { L, R, Ls, Rs } },
    { kAudioChannelLayoutTag_Pentagonal, "Pentagonal", { L, R, Ls, Rs, C } },
    { kAudioChannelLayoutTag_Hexagonal, "Hexagonal", { L, R, Ls, Rs, C, Cs } },
    { kAudioChannelLayoutTag_Octagonal, "Octagonal", { L, R, Ls, Rs, C, Cs, Lw, Rw } },
    { kAudioChannelLayoutTag_MPEG_3_0_A, "3.0 (L R C)", { L, R, C } },
    { kAudioChannelLayoutTag_MPEG_3_0_B, "3.0 (C L R)", { C, L, R } },
    { kAudioChannelLayoutTag_MPEG_4_0_A, "4.0 (L R C Cs)", { L, R, C, Cs } },
    { kAudioChannelLayoutTag_MPEG_4_0_B, "4.0 (C L R Cs)", { C, L, R, Cs } },
    { kAudioChannelLayoutTag_MPEG_5_0_A, "5.0 (L R C Ls Rs)", { L, R, C, Ls, Rs } },
    { kAudioChannelLayoutTag_MPEG_5_0_B, "5.0 (L R Ls Rs C)", { L, R, Ls, Rs, C } },
    { kAudioChannelLayoutTag_MPEG_5_0_C, "5.0 (L C R Ls Rs)", { L, C, R, Ls, Rs } },
    { kAudioChannelLayoutTag_MPEG_5_0_D, "5.0 (C L R Ls Rs)", { C, L, R, Ls, Rs } },
    { kAudioChannelLayoutTag_MPEG_5_1_A, "5.1 (L R C LFE Ls Rs)", { L, R, C, LFE, Ls, Rs } },
    { kAudioChannelLayoutTag_MPEG_5_1_B, "5.1 (L R Ls Rs C LFE)", { L, R, Ls, Rs, C, LFE } },
    { kAudioChannelLayoutTag_MPEG_5_1_C, "5.1 (L C R Ls Rs LFE)", { L, C, R, Ls, Rs, LFE } },
    { kAudioChannelLayoutTag_MPEG_5_1_D, "5.1 (C L R Ls Rs LFE)", { C, L, R, Ls, Rs, LFE } },
    { kAudioChannelLayoutTag_MPEG_6_1_A, "6.1 (L R C LFE Ls Rs Cs)", { L, R, C, LFE, Ls, Rs, Cs } },
    { kAudioChannelLayoutTag_MPEG_7_1_A, "7.1 (L R C LFE Ls Rs Lc Rc)", { L, R, C, LFE, Ls, Rs, Lc, Rc } },
    { kAudioChannelLayoutTag_MPEG_7_1_B, "7.1 (C Lc Rc L R Ls Rs LFE)", { C, Lc, Rc, L, R, Ls, Rs, LFE } },
    { kAudioChannelLayoutTag_MPEG_7_1_C, "7.1 (L R C LFE Ls Rs Rls Rrs)", { L, R, C, LFE, Ls, Rs, Rls, Rrs } },
    { kAudioChannelLayoutTag_ITU_2_1, "2.1 (L R Cs)", { L, R, Cs } },
    { kAudioChannelLayoutTag_ITU_2_2, "2.2 (L R Ls Rs)", { L, R, Ls, Rs } },
    { kAudioChannelLayoutTag_AudioUnit_6_0, "6.0 (L R Ls Rs C Cs)", { L, R, Ls, Rs, C, Cs } },
    { kAudioChannelLayoutTag_AudioUnit_7_0, "7.0 (L R Ls Rs C Rls Rrs)", { L, R, Ls, Rs, C, Rls, Rrs } },
    { kAudioChannelLayoutTag_AudioUnit_7_0_Front, "7.0 (L R Ls Rs C Lc Rc)", { L, R, Ls, Rs, C, Lc, Rc } },
    { kAudioChannelLayoutTag_AAC_6_0, "6.0 (C L R Ls Rs Cs)", { C, L, R, Ls, Rs, Cs } },
    { kAudioChannelLayoutTag_AAC_6_1, "6.1 (C L R Ls Rs Cs LFE)", { C, L, R, Ls, Rs, Cs, LFE } },
    { kAudioChannelLayoutTag_AAC_7_0, "7.0 (C L R Ls Rs Rls Rrs)", { C, L, R, Ls, Rs, Rls, Rrs } },
    { kAudioChannelLayoutTag_AAC_Octagonal, "Octagonal (C L R Ls Rs Rls Rrs Cs)", { C, L, R, Ls, Rs, Rls, Rrs, Cs } },
};
#define kLayoutCount (sizeof(layouts) / sizeof(layouts[0]))

static const LayoutEntry *layoutEntry(AudioChannelLayoutTag tag) {
    for (size_t i = 0; i < kLayoutCount; i++)
        if (layouts[i].tag == tag)
            return &layouts[i];
    return NULL;
}

static UInt32 layoutSize(UInt32 descriptions) {
    return (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) +
                    descriptions * sizeof(AudioChannelDescription));
}

/* A layout's channels as labels; false when it cannot be read. */
static Boolean labelsOf(const AudioChannelLayout *layout, UInt32 size, AudioChannelLabel *labels, UInt32 *count) {
    UInt32 tag = layout->mChannelLayoutTag;

    *count = 0;
    if (tag == kAudioChannelLayoutTag_UseChannelDescriptions) {
        if (size < layoutSize(layout->mNumberChannelDescriptions) || layout->mNumberChannelDescriptions > 64)
            return false;
        for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++)
            labels[(*count)++] = layout->mChannelDescriptions[i].mChannelLabel;
        return true;
    }
    if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
        for (UInt32 b = 0; b < 18; b++)
            if (layout->mChannelBitmap & (1u << b))
                labels[(*count)++] = b + 1;
        return true;
    }
    if ((tag & 0xFFFF0000u) == kAudioChannelLayoutTag_DiscreteInOrder) {
        for (UInt32 i = 0; i < AudioChannelLayoutTag_GetNumberOfChannels(tag) && i < 64; i++)
            labels[(*count)++] = kAudioChannelLabel_Discrete_0 | i;
        return true;
    }
    {
        const LayoutEntry *e = layoutEntry(tag);
        if (e == NULL)
            return false;
        for (UInt32 i = 0; i < AudioChannelLayoutTag_GetNumberOfChannels(tag); i++)
            labels[(*count)++] = e->labels[i];
        return true;
    }
}

static const char *channelName(AudioChannelLabel label, Boolean shortName) {
    switch (label) {
    case kAudioChannelLabel_Left: return shortName ? "L" : "Left";
    case kAudioChannelLabel_Right: return shortName ? "R" : "Right";
    case kAudioChannelLabel_Center: return shortName ? "C" : "Center";
    case kAudioChannelLabel_LFEScreen: return shortName ? "LFE" : "LFE Screen";
    case kAudioChannelLabel_LeftSurround: return shortName ? "Ls" : "Left Surround";
    case kAudioChannelLabel_RightSurround: return shortName ? "Rs" : "Right Surround";
    case kAudioChannelLabel_LeftCenter: return shortName ? "Lc" : "Left Center";
    case kAudioChannelLabel_RightCenter: return shortName ? "Rc" : "Right Center";
    case kAudioChannelLabel_CenterSurround: return shortName ? "Cs" : "Center Surround";
    case kAudioChannelLabel_RearSurroundLeft: return shortName ? "Rls" : "Rear Surround Left";
    case kAudioChannelLabel_RearSurroundRight: return shortName ? "Rrs" : "Rear Surround Right";
    case kAudioChannelLabel_LeftWide: return shortName ? "Lw" : "Left Wide";
    case kAudioChannelLabel_RightWide: return shortName ? "Rw" : "Right Wide";
    case kAudioChannelLabel_Mono: return shortName ? "M" : "Mono";
    case kAudioChannelLabel_Headphones_Left: return shortName ? "HL" : "Headphones Left";
    case kAudioChannelLabel_Headphones_Right: return shortName ? "HR" : "Headphones Right";
    case kAudioChannelLabel_LeftTotal: return shortName ? "Lt" : "Left Total";
    case kAudioChannelLabel_RightTotal: return shortName ? "Rt" : "Right Total";
    default: return NULL;
    }
}

/* ---- The properties ------------------------------------------------------------ */

static OSStatus putBytes(void *out, UInt32 *ioSize, const void *data, UInt32 size, Boolean sizeOnly) {
    if (!sizeOnly) {
        if (*ioSize < size)
            return kAudioFormatBadPropertySizeError;
        memcpy(out, data, size);
    }
    *ioSize = size;
    return noErr;
}

static OSStatus putLayout(void *out, UInt32 *ioSize, AudioChannelLayoutTag tag, const AudioChannelLabel *labels,
                          UInt32 count, Boolean sizeOnly) {
    UInt32 size = layoutSize(count);
    AudioChannelLayout *l;

    if (sizeOnly) {
        *ioSize = size;
        return noErr;
    }
    if (*ioSize < size)
        return kAudioFormatBadPropertySizeError;
    l = out;
    memset(l, 0, size);
    l->mChannelLayoutTag = tag;
    l->mNumberChannelDescriptions = count;
    for (UInt32 i = 0; i < count; i++)
        l->mChannelDescriptions[i].mChannelLabel = labels[i];
    *ioSize = size;
    return noErr;
}

static AudioChannelLayoutTag defaultTag(UInt32 channels) {
    switch (channels) {
    case 1: return kAudioChannelLayoutTag_Mono;
    case 2: return kAudioChannelLayoutTag_Stereo;
    case 3: return kAudioChannelLayoutTag_MPEG_3_0_A;
    case 4: return kAudioChannelLayoutTag_Quadraphonic;
    case 5: return kAudioChannelLayoutTag_MPEG_5_0_A;
    case 6: return kAudioChannelLayoutTag_MPEG_5_1_A;
    case 7: return kAudioChannelLayoutTag_MPEG_6_1_A;
    case 8: return kAudioChannelLayoutTag_MPEG_7_1_C;
    default: return kAudioChannelLayoutTag_DiscreteInOrder | channels;
    }
}

static OSStatus property(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *ioSize, void *out,
                         Boolean sizeOnly) {
    AudioChannelLabel labels[64];
    UInt32 count;

    switch (id) {
    case kAudioFormatProperty_FormatInfo: {
        AudioStreamBasicDescription d;
        OSStatus status;
        if (sizeOnly) {
            *ioSize = sizeof(d);
            return noErr;
        }
        if (*ioSize < sizeof(d))
            return kAudioFormatBadPropertySizeError;
        memcpy(&d, out, sizeof(d));
        status = formatInfo(&d, specSize > 0 ? spec : NULL, specSize);
        if (status != noErr)
            return status;
        memcpy(out, &d, sizeof(d));
        *ioSize = sizeof(d);
        return noErr;
    }
    case kAudioFormatProperty_FormatList: {
        const AudioFormatInfo *info = spec;
        AudioFormatListItem item;
        OSStatus status;
        if (specSize < sizeof(*info))
            return kAudioFormatBadSpecifierSizeError;
        item.mASBD = info->mASBD;
        status = formatInfo(&item.mASBD, info->mMagicCookie, info->mMagicCookieSize);
        if (status != noErr)
            return status;
        item.mChannelLayoutTag = defaultTag(item.mASBD.mChannelsPerFrame);
        return putBytes(out, ioSize, &item, sizeof(item), sizeOnly);
    }
    case kAudioFormatProperty_ASBDFromESDS: {
        AudioStreamBasicDescription d;
        memset(&d, 0, sizeof(d));
        d.mFormatID = kAudioFormatMPEG4AAC;
        if (spec == NULL || specSize == 0)
            return kAudioFormatBadSpecifierSizeError;
        formatInfo(&d, spec, specSize);
        if (d.mSampleRate == 0)
            return kAudioFormatUnsupportedDataFormatError;
        return putBytes(out, ioSize, &d, sizeof(d), sizeOnly);
    }
    case kAudioFormatProperty_FirstPlayableFormatFromList: {
        UInt32 n = specSize / sizeof(AudioFormatListItem), index = 0xFFFFFFFF;
        const AudioFormatListItem *list = spec;
        for (UInt32 i = 0; i < n; i++)
            if (formatEntry(list[i].mASBD.mFormatID) != NULL) {
                index = i;
                break;
            }
        if (index == 0xFFFFFFFF)
            return kAudioFormatUnsupportedDataFormatError;
        return putBytes(out, ioSize, &index, sizeof(index), sizeOnly);
    }
    case kAudioFormatProperty_FormatName: {
        CFStringRef name;
        if (specSize < sizeof(AudioStreamBasicDescription))
            return kAudioFormatBadSpecifierSizeError;
        if (sizeOnly) {
            *ioSize = sizeof(CFStringRef);
            return noErr;
        }
        name = formatName(spec);
        if (name == NULL)
            return kAudioFormatUnknownFormatError;
        if (*ioSize < sizeof(name)) {
            CFRelease(name);
            return kAudioFormatBadPropertySizeError;
        }
        *(CFStringRef *)out = name;
        *ioSize = sizeof(name);
        return noErr;
    }
    case kAudioFormatProperty_FormatIsVBR:
    case kAudioFormatProperty_FormatIsExternallyFramed:
    case kAudioFormatProperty_FormatIsEncrypted:
    case kAudioFormatProperty_FormatEmploysDependentPackets: {
        const AudioStreamBasicDescription *d = spec;
        const FormatEntry *e;
        UInt32 v;
        if (specSize < sizeof(*d))
            return kAudioFormatBadSpecifierSizeError;
        e = formatEntry(d->mFormatID);
        if (e == NULL)
            return kAudioFormatUnknownFormatError;
        v = id == kAudioFormatProperty_FormatIsVBR ? e->vbr
            : id == kAudioFormatProperty_FormatIsExternallyFramed ? e->externallyFramed : 0;
        return putBytes(out, ioSize, &v, sizeof(v), sizeOnly);
    }
    case kAudioFormatProperty_EncodeFormatIDs: {
        UInt32 ids[] = { kAudioFormatLinearPCM };
        return putBytes(out, ioSize, ids, sizeof(ids), sizeOnly);
    }
    case kAudioFormatProperty_DecodeFormatIDs: {
        /* Linear PCM, and what FFmpeg decodes for AudioConverter. */
        static const UInt32 compressed[] = { kAudioFormatMPEG4AAC, kAudioFormatMPEG4AAC_HE, kAudioFormatMPEG4AAC_HE_V2,
            kAudioFormatMPEG4AAC_LD, kAudioFormatMPEG4AAC_ELD, kAudioFormatMPEGLayer1, kAudioFormatMPEGLayer2,
            kAudioFormatMPEGLayer3, kAudioFormatAppleLossless, kAudioFormatFLAC, kAudioFormatOpus,
            kAudioFormatAC3, kAudioFormatEnhancedAC3, kAudioFormatULaw, kAudioFormatALaw,
            kAudioFormatAMR, kAudioFormatAMR_WB };
        UInt32 ids[1 + sizeof(compressed) / sizeof(compressed[0])], n = 0;
        ids[n++] = kAudioFormatLinearPCM;
        for (UInt32 i = 0; i < sizeof(compressed) / sizeof(compressed[0]); i++)
            if (_ATCodecCanDecode(compressed[i]))
                ids[n++] = compressed[i];
        return putBytes(out, ioSize, ids, n * sizeof(UInt32), sizeOnly);
    }
    case kAudioFormatProperty_ChannelLayoutForTag: {
        AudioChannelLayoutTag tag;
        AudioChannelLayout l;
        if (specSize < sizeof(tag))
            return kAudioFormatBadSpecifierSizeError;
        tag = *(const AudioChannelLayoutTag *)spec;
        memset(&l, 0, sizeof(l));
        l.mChannelLayoutTag = tag;
        if (tag == kAudioChannelLayoutTag_UseChannelDescriptions || tag == kAudioChannelLayoutTag_UseChannelBitmap ||
            !labelsOf(&l, sizeof(l), labels, &count))
            return kAudioFormatUnknownFormatError;
        return putLayout(out, ioSize, tag, labels, count, sizeOnly);
    }
    case kAudioFormatProperty_ChannelLayoutForBitmap: {
        AudioChannelLayout l;
        if (specSize < sizeof(UInt32))
            return kAudioFormatBadSpecifierSizeError;
        memset(&l, 0, sizeof(l));
        l.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
        l.mChannelBitmap = *(const UInt32 *)spec;
        labelsOf(&l, sizeof(l), labels, &count);
        return putLayout(out, ioSize, kAudioChannelLayoutTag_UseChannelDescriptions, labels, count, sizeOnly);
    }
    case kAudioFormatProperty_BitmapForLayoutTag: {
        AudioChannelLayout l;
        UInt32 bitmap = 0;
        if (specSize < sizeof(AudioChannelLayoutTag))
            return kAudioFormatBadSpecifierSizeError;
        memset(&l, 0, sizeof(l));
        l.mChannelLayoutTag = *(const AudioChannelLayoutTag *)spec;
        if (!labelsOf(&l, sizeof(l), labels, &count))
            return kAudioFormatUnknownFormatError;
        for (UInt32 i = 0; i < count; i++) {
            if (labels[i] < 1 || labels[i] > 18)
                return kAudioFormatUnsupportedDataFormatError;
            bitmap |= 1u << (labels[i] - 1);
        }
        return putBytes(out, ioSize, &bitmap, sizeof(bitmap), sizeOnly);
    }
    case kAudioFormatProperty_NumberOfChannelsForLayout:
    case kAudioFormatProperty_ValidateChannelLayout: {
        if (specSize < layoutSize(0))
            return kAudioFormatBadSpecifierSizeError;
        if (!labelsOf(spec, specSize, labels, &count))
            return kAudioFormatUnknownFormatError;
        if (id == kAudioFormatProperty_ValidateChannelLayout) {
            if (sizeOnly)
                *ioSize = 0;
            return noErr;
        }
        return putBytes(out, ioSize, &count, sizeof(count), sizeOnly);
    }
    case kAudioFormatProperty_TagForChannelLayout: {
        AudioChannelLabel want[64];
        UInt32 n;
        AudioChannelLayoutTag tag = kAudioChannelLayoutTag_Unknown;
        if (specSize < layoutSize(0) || !labelsOf(spec, specSize, want, &n))
            return kAudioFormatBadSpecifierSizeError;
        if (((const AudioChannelLayout *)spec)->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions &&
            ((const AudioChannelLayout *)spec)->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelBitmap) {
            tag = ((const AudioChannelLayout *)spec)->mChannelLayoutTag;
        } else {
            for (size_t i = 0; i < kLayoutCount && tag == kAudioChannelLayoutTag_Unknown; i++)
                if (AudioChannelLayoutTag_GetNumberOfChannels(layouts[i].tag) == n &&
                    memcmp(layouts[i].labels, want, n * sizeof(AudioChannelLabel)) == 0)
                    tag = layouts[i].tag;
        }
        if (tag == kAudioChannelLayoutTag_Unknown)
            return kAudioFormatUnsupportedDataFormatError;
        return putBytes(out, ioSize, &tag, sizeof(tag), sizeOnly);
    }
    case kAudioFormatProperty_AreChannelLayoutsEquivalent: {
        const AudioChannelLayout *const *pair = spec;
        AudioChannelLabel other[64];
        UInt32 n, m, v;
        if (specSize < 2 * sizeof(void *))
            return kAudioFormatBadSpecifierSizeError;
        if (!labelsOf(pair[0], layoutSize(pair[0]->mNumberChannelDescriptions), labels, &n) ||
            !labelsOf(pair[1], layoutSize(pair[1]->mNumberChannelDescriptions), other, &m))
            return kAudioFormatUnknownFormatError;
        v = n == m && memcmp(labels, other, n * sizeof(AudioChannelLabel)) == 0;
        return putBytes(out, ioSize, &v, sizeof(v), sizeOnly);
    }
    case kAudioFormatProperty_TagsForNumberOfChannels: {
        AudioChannelLayoutTag tags[kLayoutCount];
        UInt32 n = 0, channels;
        if (specSize < sizeof(UInt32))
            return kAudioFormatBadSpecifierSizeError;
        channels = *(const UInt32 *)spec;
        for (size_t i = 0; i < kLayoutCount; i++)
            if (AudioChannelLayoutTag_GetNumberOfChannels(layouts[i].tag) == channels)
                tags[n++] = layouts[i].tag;
        return putBytes(out, ioSize, tags, n * sizeof(AudioChannelLayoutTag), sizeOnly);
    }
    case kAudioFormatProperty_ChannelLayoutName:
    case kAudioFormatProperty_ChannelLayoutSimpleName: {
        const AudioChannelLayout *l = spec;
        const LayoutEntry *e;
        char name[96];
        if (specSize < layoutSize(0))
            return kAudioFormatBadSpecifierSizeError;
        e = layoutEntry(l->mChannelLayoutTag);
        if (e != NULL)
            snprintf(name, sizeof(name), "%s", e->name);
        else if (labelsOf(l, specSize, labels, &count))
            snprintf(name, sizeof(name), "%u channels", (unsigned)count);
        else
            return kAudioFormatUnknownFormatError;
        if (id == kAudioFormatProperty_ChannelLayoutSimpleName) {
            char *paren = strstr(name, " (");
            if (paren != NULL)
                *paren = 0;
        }
        if (sizeOnly) {
            *ioSize = sizeof(CFStringRef);
            return noErr;
        }
        if (*ioSize < sizeof(CFStringRef))
            return kAudioFormatBadPropertySizeError;
        *(CFStringRef *)out = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        *ioSize = sizeof(CFStringRef);
        return noErr;
    }
    case kAudioFormatProperty_ChannelName:
    case kAudioFormatProperty_ChannelShortName: {
        const AudioChannelDescription *d = spec;
        const char *name;
        char discrete[32];
        if (specSize < sizeof(*d))
            return kAudioFormatBadSpecifierSizeError;
        name = channelName(d->mChannelLabel, id == kAudioFormatProperty_ChannelShortName);
        if (name == NULL && (d->mChannelLabel & 0xFFFF0000u) == kAudioChannelLabel_Discrete_0) {
            snprintf(discrete, sizeof(discrete), "Discrete %u", (unsigned)(d->mChannelLabel & 0xFFFF));
            name = discrete;
        }
        if (name == NULL)
            return kAudioFormatUnknownFormatError;
        if (sizeOnly) {
            *ioSize = sizeof(CFStringRef);
            return noErr;
        }
        if (*ioSize < sizeof(CFStringRef))
            return kAudioFormatBadPropertySizeError;
        *(CFStringRef *)out = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        *ioSize = sizeof(CFStringRef);
        return noErr;
    }
    default:
        return kAudioFormatUnsupportedPropertyError;
    }
}

OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *outSize) {
    UInt32 size = 0;
    OSStatus status;

    if (outSize == NULL)
        return kAudio_ParamError;
    status = property(id, specSize, spec, &size, NULL, true);
    if (status == noErr)
        *outSize = size;
    return status;
}

OSStatus AudioFormatGetProperty(AudioFormatPropertyID id, UInt32 specSize, const void *spec, UInt32 *ioSize,
                                void *out) {
    if (ioSize == NULL || (out == NULL && *ioSize != 0))
        return kAudio_ParamError;
    return property(id, specSize, spec, ioSize, out, false);
}
