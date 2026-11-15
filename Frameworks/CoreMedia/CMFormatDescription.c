/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Format descriptions. An H.264 or HEVC description keeps its parameter
 * sets as the codec's configuration record ('avcC', 'hvcC') among its
 * sample description extension atoms, as the files carry them; its
 * dimensions come from the sequence parameter set (the coded size less
 * the cropping or conformance window).
 */

#include "CMFormatDescription.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>

struct opaqueCMFormatDescription {
    CFRuntimeBase base;
    CMMediaType mediaType;
    FourCharCode subType;
    CFDictionaryRef extensions;         /* never NULL: empty when none */
    /* audio */
    Boolean hasAudio;
    AudioStreamBasicDescription asbd;
    AudioChannelLayout *layout;
    size_t layoutSize;
    void *cookie;
    size_t cookieSize;
    AudioFormatListItem formatItem;
    /* video */
    CMVideoDimensions dimensions;
};

static CFTypeID typeID;

static void finalize(CFTypeRef cf) {
    struct opaqueCMFormatDescription *d = (struct opaqueCMFormatDescription *)cf;
    if (d->extensions != NULL)
        CFRelease(d->extensions);
    free(d->layout);
    free(d->cookie);
}

static CFStringRef describe(CFTypeRef cf) {
    const struct opaqueCMFormatDescription *d = cf;
    char t[5] = { (char)(d->mediaType >> 24), (char)(d->mediaType >> 16), (char)(d->mediaType >> 8), (char)d->mediaType, 0 };
    char s[5] = { (char)(d->subType >> 24), (char)(d->subType >> 16), (char)(d->subType >> 8), (char)d->subType, 0 };
    if (d->mediaType == kCMMediaType_Video)
        return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CMVideoFormatDescription %p '%s' %dx%d extensions=%@>"), cf,
                                        s, d->dimensions.width, d->dimensions.height, d->extensions);
    if (d->hasAudio)
        return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CMAudioFormatDescription %p '%s' %g Hz, %u channels>"), cf,
                                        s, d->asbd.mSampleRate, (unsigned)d->asbd.mChannelsPerFrame);
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CMFormatDescription %p '%s' '%s' extensions=%@>"), cf, t, s,
                                    d->extensions);
}

static Boolean equal(CFTypeRef a, CFTypeRef b) {
    return CMFormatDescriptionEqual(a, b);
}

static const CFRuntimeClass formatDescriptionClass = { 0, "CMFormatDescription", NULL, NULL, finalize, equal, NULL,
                                                       NULL, describe };

CFTypeID CMFormatDescriptionGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&formatDescriptionClass); });
    return typeID;
}

static struct opaqueCMFormatDescription *create(CFAllocatorRef allocator, CMMediaType type, FourCharCode subType,
                                                CFDictionaryRef extensions) {
    size_t extra = sizeof(struct opaqueCMFormatDescription) - sizeof(CFRuntimeBase);
    struct opaqueCMFormatDescription *d = (struct opaqueCMFormatDescription *)_CFRuntimeCreateInstance(
        allocator, CMFormatDescriptionGetTypeID(), extra, NULL);
    if (d == NULL)
        return NULL;
    d->mediaType = type;
    d->subType = subType;
    d->extensions = extensions != NULL ? CFDictionaryCreateCopy(allocator, extensions)
                                       : CFDictionaryCreate(allocator, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    return d;
}

OSStatus CMFormatDescriptionCreate(CFAllocatorRef allocator, CMMediaType mediaType, FourCharCode subType,
                                   CFDictionaryRef extensions, CMFormatDescriptionRef *out) {
    if (out == NULL || (extensions != NULL && CFGetTypeID(extensions) != CFDictionaryGetTypeID()))
        return kCMFormatDescriptionError_InvalidParameter;
    *out = create(allocator, mediaType, subType, extensions);
    return *out != NULL ? noErr : kCMFormatDescriptionError_AllocationFailed;
}

CMMediaType CMFormatDescriptionGetMediaType(CMFormatDescriptionRef d) {
    return d != NULL ? d->mediaType : 0;
}

FourCharCode CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef d) {
    return d != NULL ? d->subType : 0;
}

CFDictionaryRef CMFormatDescriptionGetExtensions(CMFormatDescriptionRef d) {
    return d != NULL && CFDictionaryGetCount(d->extensions) > 0 ? d->extensions : NULL;
}

CFPropertyListRef CMFormatDescriptionGetExtension(CMFormatDescriptionRef d, CFStringRef key) {
    return d != NULL && key != NULL ? CFDictionaryGetValue(d->extensions, key) : NULL;
}

/* ---- Equality ---- */

static Boolean ignored(CFTypeRef keys, CFTypeRef key) {
    if (keys == NULL)
        return false;
    if (CFGetTypeID(keys) == CFStringGetTypeID())
        return CFEqual(keys, key);
    if (CFGetTypeID(keys) == CFArrayGetTypeID())
        return CFArrayContainsValue(keys, CFRangeMake(0, CFArrayGetCount(keys)), key);
    return false;
}

/* The dictionary less the ignored keys; the atoms' dictionary less its
   ignored atoms. */
static CFDictionaryRef without(CFDictionaryRef d, CFTypeRef keys, CFTypeRef atomKeys) {
    CFMutableDictionaryRef m = CFDictionaryCreateMutableCopy(NULL, 0, d);
    CFIndex n = CFDictionaryGetCount(d);
    const void *k[n > 0 ? n : 1], *v[n > 0 ? n : 1];
    CFDictionaryGetKeysAndValues(d, k, v);
    for (CFIndex i = 0; i < n; i++)
        if (ignored(keys, k[i]))
            CFDictionaryRemoveValue(m, k[i]);
    CFTypeRef atoms = CFDictionaryGetValue(m, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    if (atomKeys != NULL && atoms != NULL && CFGetTypeID(atoms) == CFDictionaryGetTypeID()) {
        CFDictionaryRef trimmed = without(atoms, atomKeys, NULL);
        CFDictionarySetValue(m, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, trimmed);
        CFRelease(trimmed);
    }
    return m;
}

static Boolean sameContents(CMFormatDescriptionRef a, CMFormatDescriptionRef b) {
    if (a->mediaType != b->mediaType || a->subType != b->subType || a->hasAudio != b->hasAudio)
        return false;
    if (a->hasAudio && (memcmp(&a->asbd, &b->asbd, sizeof(a->asbd)) != 0 || a->layoutSize != b->layoutSize ||
                        a->cookieSize != b->cookieSize ||
                        (a->layoutSize > 0 && memcmp(a->layout, b->layout, a->layoutSize) != 0) ||
                        (a->cookieSize > 0 && memcmp(a->cookie, b->cookie, a->cookieSize) != 0)))
        return false;
    return a->dimensions.width == b->dimensions.width && a->dimensions.height == b->dimensions.height;
}

Boolean CMFormatDescriptionEqual(CMFormatDescriptionRef a, CMFormatDescriptionRef b) {
    if (a == b)
        return true;
    if (a == NULL || b == NULL)
        return false;
    return sameContents(a, b) && CFEqual(a->extensions, b->extensions);
}

Boolean CMFormatDescriptionEqualIgnoringExtensionKeys(CMFormatDescriptionRef a, CMFormatDescriptionRef b,
                                                      CFTypeRef keys, CFTypeRef atomKeys) {
    if (a == NULL || b == NULL)
        return a == b;
    if (!sameContents(a, b))
        return false;
    CFDictionaryRef x = without(a->extensions, keys, atomKeys), y = without(b->extensions, keys, atomKeys);
    Boolean same = CFEqual(x, y);
    CFRelease(x);
    CFRelease(y);
    return same;
}

/* ---- Audio ---- */

OSStatus CMAudioFormatDescriptionCreate(CFAllocatorRef allocator, const AudioStreamBasicDescription *asbd,
                                        size_t layoutSize, const AudioChannelLayout *layout, size_t cookieSize,
                                        const void *cookie, CFDictionaryRef extensions, CMAudioFormatDescriptionRef *out) {
    if (out == NULL || asbd == NULL || asbd->mSampleRate < 0 || asbd->mFormatID == 0 ||
        (layoutSize > 0 && layout == NULL) || (cookieSize > 0 && cookie == NULL))
        return kCMFormatDescriptionError_InvalidParameter;
    struct opaqueCMFormatDescription *d = create(allocator, kCMMediaType_Audio, asbd->mFormatID, extensions);
    if (d == NULL)
        return kCMFormatDescriptionError_AllocationFailed;
    d->hasAudio = true;
    d->asbd = *asbd;
    if (layoutSize > 0) {
        d->layout = malloc(layoutSize);
        memcpy(d->layout, layout, layoutSize);
        d->layoutSize = layoutSize;
    }
    if (cookieSize > 0) {
        d->cookie = malloc(cookieSize);
        memcpy(d->cookie, cookie, cookieSize);
        d->cookieSize = cookieSize;
    }
    d->formatItem.mASBD = *asbd;
    d->formatItem.mChannelLayoutTag = layoutSize > 0 ? layout->mChannelLayoutTag : 0;
    *out = d;
    return noErr;
}

const AudioStreamBasicDescription *CMAudioFormatDescriptionGetStreamBasicDescription(CMAudioFormatDescriptionRef d) {
    return d != NULL && d->hasAudio ? &d->asbd : NULL;
}

const void *CMAudioFormatDescriptionGetMagicCookie(CMAudioFormatDescriptionRef d, size_t *size) {
    if (size != NULL)
        *size = d != NULL ? d->cookieSize : 0;
    return d != NULL ? d->cookie : NULL;
}

const AudioChannelLayout *CMAudioFormatDescriptionGetChannelLayout(CMAudioFormatDescriptionRef d, size_t *size) {
    if (size != NULL)
        *size = d != NULL ? d->layoutSize : 0;
    return d != NULL ? d->layout : NULL;
}

/* One format: the stream's own, with its layout's tag. */
const AudioFormatListItem *CMAudioFormatDescriptionGetFormatList(CMAudioFormatDescriptionRef d, size_t *size) {
    if (size != NULL)
        *size = d != NULL && d->hasAudio ? sizeof(AudioFormatListItem) : 0;
    return d != NULL && d->hasAudio ? &d->formatItem : NULL;
}

const AudioFormatListItem *CMAudioFormatDescriptionGetRichestDecodableFormat(CMAudioFormatDescriptionRef d) {
    return CMAudioFormatDescriptionGetFormatList(d, NULL);
}

const AudioFormatListItem *CMAudioFormatDescriptionGetMostCompatibleFormat(CMAudioFormatDescriptionRef d) {
    return CMAudioFormatDescriptionGetFormatList(d, NULL);
}

Boolean CMAudioFormatDescriptionEqual(CMAudioFormatDescriptionRef a, CMAudioFormatDescriptionRef b,
                                      CMAudioFormatDescriptionMask mask, CMAudioFormatDescriptionMask *equalOut) {
    CMAudioFormatDescriptionMask same = 0;
    if (a != NULL && b != NULL && a->hasAudio && b->hasAudio) {
        if (memcmp(&a->asbd, &b->asbd, sizeof(a->asbd)) == 0)
            same |= kCMAudioFormatDescriptionMask_StreamBasicDescription;
        if (a->cookieSize == b->cookieSize && (a->cookieSize == 0 || memcmp(a->cookie, b->cookie, a->cookieSize) == 0))
            same |= kCMAudioFormatDescriptionMask_MagicCookie;
        if (a->layoutSize == b->layoutSize && (a->layoutSize == 0 || memcmp(a->layout, b->layout, a->layoutSize) == 0))
            same |= kCMAudioFormatDescriptionMask_ChannelLayout;
        if (CFEqual(a->extensions, b->extensions))
            same |= kCMAudioFormatDescriptionMask_Extensions;
    }
    same &= mask;
    if (equalOut != NULL)
        *equalOut = same;
    return same == (mask & kCMAudioFormatDescriptionMask_All);
}

/* ---- Video ---- */

OSStatus CMVideoFormatDescriptionCreate(CFAllocatorRef allocator, CMVideoCodecType codecType, int32_t width,
                                        int32_t height, CFDictionaryRef extensions, CMVideoFormatDescriptionRef *out) {
    if (out == NULL || width < 0 || height < 0)
        return kCMFormatDescriptionError_InvalidParameter;
    struct opaqueCMFormatDescription *d = create(allocator, kCMMediaType_Video, codecType, extensions);
    if (d == NULL)
        return kCMFormatDescriptionError_AllocationFailed;
    d->dimensions = (CMVideoDimensions){ width, height };
    *out = d;
    return noErr;
}

CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef d) {
    return d != NULL && d->mediaType == kCMMediaType_Video ? d->dimensions : (CMVideoDimensions){ 0, 0 };
}

static double number(CFDictionaryRef d, CFStringRef key, double otherwise) {
    CFTypeRef v = d != NULL && CFGetTypeID(d) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(d, key) : NULL;
    double x = otherwise;
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberDoubleType, &x);
    return x;
}

CGRect CMVideoFormatDescriptionGetCleanAperture(CMVideoFormatDescriptionRef d, Boolean originIsAtTopLeft) {
    CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(d);
    CFDictionaryRef ca = d != NULL ? CFDictionaryGetValue(d->extensions, kCMFormatDescriptionExtension_CleanAperture) : NULL;
    double w = number(ca, kCMFormatDescriptionKey_CleanApertureWidth, dim.width);
    double h = number(ca, kCMFormatDescriptionKey_CleanApertureHeight, dim.height);
    double x = (dim.width - w) / 2 + number(ca, kCMFormatDescriptionKey_CleanApertureHorizontalOffset, 0);
    double y = (dim.height - h) / 2 + number(ca, kCMFormatDescriptionKey_CleanApertureVerticalOffset, 0);
    if (!originIsAtTopLeft)
        y = dim.height - y - h;
    return CGRectMake(x, y, w, h);
}

CGSize CMVideoFormatDescriptionGetPresentationDimensions(CMVideoFormatDescriptionRef d, Boolean usePixelAspectRatio,
                                                         Boolean useCleanAperture) {
    CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(d);
    CGSize size = CGSizeMake(dim.width, dim.height);
    if (useCleanAperture)
        size = CMVideoFormatDescriptionGetCleanAperture(d, true).size;
    if (usePixelAspectRatio && d != NULL) {
        CFDictionaryRef par = CFDictionaryGetValue(d->extensions, kCMFormatDescriptionExtension_PixelAspectRatio);
        double hs = number(par, kCMFormatDescriptionKey_PixelAspectRatioHorizontalSpacing, 1);
        double vs = number(par, kCMFormatDescriptionKey_PixelAspectRatioVerticalSpacing, 1);
        if (hs > 0 && vs > 0)
            size.width = size.width * hs / vs;
    }
    return size;
}

CFArrayRef CMVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers(void) {
    static CFArrayRef keys;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        const void *k[] = {
            kCMFormatDescriptionExtension_CleanAperture, kCMFormatDescriptionExtension_FieldCount,
            kCMFormatDescriptionExtension_FieldDetail, kCMFormatDescriptionExtension_PixelAspectRatio,
            kCMFormatDescriptionExtension_ColorPrimaries, kCMFormatDescriptionExtension_TransferFunction,
            kCMFormatDescriptionExtension_GammaLevel, kCMFormatDescriptionExtension_YCbCrMatrix,
            kCMFormatDescriptionExtension_ChromaLocationTopField, kCMFormatDescriptionExtension_ChromaLocationBottomField,
            kCMFormatDescriptionExtension_MasteringDisplayColorVolume,
            kCMFormatDescriptionExtension_ContentLightLevelInfo,
        };
        keys = CFArrayCreate(NULL, k, sizeof(k) / sizeof(k[0]), &kCFTypeArrayCallBacks);
    });
    return keys;
}

/* The pixel format, the size and the buffer's attachments of the keys in
   common, as extensions. */
OSStatus CMVideoFormatDescriptionCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef buffer,
                                                      CMVideoFormatDescriptionRef *out) {
    if (out == NULL || buffer == NULL || CFGetTypeID(buffer) != CVPixelBufferGetTypeID())
        return kCMFormatDescriptionError_InvalidParameter;
    CFMutableDictionaryRef ext = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFArrayRef keys = CMVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers();
    for (CFIndex i = 0; i < CFArrayGetCount(keys); i++) {
        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
        CFTypeRef v = CVBufferGetAttachment(buffer, key, NULL);
        if (v != NULL)
            CFDictionarySetValue(ext, key, v);
    }
    int32_t bytesPerRow = (int32_t)CVPixelBufferGetBytesPerRow(buffer);
    CFNumberRef bpr = CFNumberCreate(NULL, kCFNumberSInt32Type, &bytesPerRow);
    if (!CVPixelBufferIsPlanar(buffer))
        CFDictionarySetValue(ext, kCMFormatDescriptionExtension_BytesPerRow, bpr);
    CFRelease(bpr);
    OSStatus err = CMVideoFormatDescriptionCreate(allocator, CVPixelBufferGetPixelFormatType(buffer),
                                                  (int32_t)CVPixelBufferGetWidth(buffer),
                                                  (int32_t)CVPixelBufferGetHeight(buffer), ext, out);
    CFRelease(ext);
    return err;
}

Boolean CMVideoFormatDescriptionMatchesImageBuffer(CMVideoFormatDescriptionRef d, CVImageBufferRef buffer) {
    if (d == NULL || buffer == NULL || CFGetTypeID(buffer) != CVPixelBufferGetTypeID())
        return false;
    if (d->subType != CVPixelBufferGetPixelFormatType(buffer) ||
        d->dimensions.width != (int32_t)CVPixelBufferGetWidth(buffer) ||
        d->dimensions.height != (int32_t)CVPixelBufferGetHeight(buffer))
        return false;
    CFArrayRef keys = CMVideoFormatDescriptionGetExtensionKeysCommonWithImageBuffers();
    for (CFIndex i = 0; i < CFArrayGetCount(keys); i++) {
        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
        CFTypeRef a = CFDictionaryGetValue(d->extensions, key), b = CVBufferGetAttachment(buffer, key, NULL);
        if ((a == NULL) != (b == NULL) || (a != NULL && !CFEqual(a, b)))
            return false;
    }
    return true;
}

/* ---- Parameter sets ---- */

/* An RBSP's bits, the emulation prevention bytes taken out. */
typedef struct {
    uint8_t *bytes;
    size_t size, bit;
    Boolean overrun;
} Bits;

static Bits rbsp(const uint8_t *nal, size_t size, size_t headerBytes) {
    Bits b = { malloc(size > 0 ? size : 1), 0, 0, false };
    int zeros = 0;
    for (size_t i = headerBytes; i < size; i++) {
        if (zeros >= 2 && nal[i] == 3) {
            zeros = 0;
            continue;
        }
        zeros = nal[i] == 0 ? zeros + 1 : 0;
        b.bytes[b.size++] = nal[i];
    }
    return b;
}

static uint32_t u(Bits *b, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++) {
        if (b->bit >= b->size * 8) {
            b->overrun = true;
            return 0;
        }
        v = (v << 1) | ((b->bytes[b->bit / 8] >> (7 - b->bit % 8)) & 1);
        b->bit++;
    }
    return v;
}

static uint32_t ue(Bits *b) {
    int zeros = 0;
    while (u(b, 1) == 0 && !b->overrun && zeros < 32)
        zeros++;
    return zeros == 0 ? 0 : ((1u << zeros) - 1) + u(b, zeros);
}

static int32_t se(Bits *b) {
    uint32_t k = ue(b);
    return (k & 1) ? (int32_t)((k + 1) / 2) : -(int32_t)(k / 2);
}

static void skipScalingList(Bits *b, int size) {
    int last = 8, next = 8;
    for (int j = 0; j < size; j++) {
        if (next != 0)
            next = (last + se(b) + 256) % 256;
        last = next == 0 ? last : next;
    }
}

/* An H.264 sequence parameter set's picture size; for the high profiles,
   its chroma format and bit depths too. */
typedef struct {
    Boolean high;
    uint32_t chroma, lumaDepth, chromaDepth;
} H264Format;

static Boolean h264Dimensions(const uint8_t *sps, size_t size, CMVideoDimensions *dim, H264Format *format) {
    Bits b = rbsp(sps, size, 1);
    uint32_t profile = u(&b, 8);
    u(&b, 16);                                   /* constraints, level */
    ue(&b);                                      /* seq_parameter_set_id */
    uint32_t chroma = 1, separate = 0, lumaDepth = 8, chromaDepth = 8;
    Boolean high = false;
    if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 || profile == 83 ||
        profile == 86 || profile == 118 || profile == 128 || profile == 138 || profile == 139 || profile == 134 ||
        profile == 135) {
        high = true;
        chroma = ue(&b);
        if (chroma == 3)
            separate = u(&b, 1);
        lumaDepth = ue(&b) + 8;
        chromaDepth = ue(&b) + 8;
        u(&b, 1);
        if (u(&b, 1))                            /* scaling matrices */
            for (int i = 0; i < (chroma != 3 ? 8 : 12); i++)
                if (u(&b, 1))
                    skipScalingList(&b, i < 6 ? 16 : 64);
    }
    ue(&b);                                      /* log2_max_frame_num */
    uint32_t pocType = ue(&b);
    if (pocType == 0)
        ue(&b);
    else if (pocType == 1) {
        u(&b, 1);
        se(&b);
        se(&b);
        uint32_t n = ue(&b);
        for (uint32_t i = 0; i < n && !b.overrun; i++)
            se(&b);
    }
    ue(&b);                                      /* max_num_ref_frames */
    u(&b, 1);
    uint32_t widthMbs = ue(&b) + 1, heightUnits = ue(&b) + 1;
    uint32_t frameMbsOnly = u(&b, 1);
    if (!frameMbsOnly)
        u(&b, 1);
    u(&b, 1);                                    /* direct_8x8_inference */
    uint32_t left = 0, right = 0, top = 0, bottom = 0;
    if (u(&b, 1)) {
        left = ue(&b);
        right = ue(&b);
        top = ue(&b);
        bottom = ue(&b);
    }
    Boolean ok = !b.overrun;
    free(b.bytes);
    if (!ok)
        return false;
    *format = (H264Format){ high, chroma, lumaDepth, chromaDepth };
    uint32_t arrayType = separate ? 0 : chroma;
    uint32_t cropX = arrayType == 0 ? 1 : (arrayType == 3 ? 1 : 2);
    uint32_t cropY = (arrayType == 0 ? 1 : (arrayType == 1 ? 2 : 1)) * (2 - frameMbsOnly);
    dim->width = (int32_t)(widthMbs * 16 - cropX * (left + right));
    dim->height = (int32_t)((2 - frameMbsOnly) * heightUnits * 16 - cropY * (top + bottom));
    return dim->width > 0 && dim->height > 0;
}

typedef struct {
    uint8_t ptl[12];              /* general profile_tier_level: 12 bytes */
    uint32_t chroma, lumaDepth, chromaDepth, maxSubLayers, temporalNesting;
    CMVideoDimensions dim;
} HEVCInfo;

static Boolean hevcInfo(const uint8_t *sps, size_t size, HEVCInfo *info) {
    Bits b = rbsp(sps, size, 2);
    u(&b, 4);                                    /* sps_video_parameter_set_id */
    info->maxSubLayers = u(&b, 3) + 1;
    info->temporalNesting = u(&b, 1);
    for (int i = 0; i < 12; i++)
        info->ptl[i] = (uint8_t)u(&b, 8);
    uint32_t profilePresent[8] = { 0 }, levelPresent[8] = { 0 };
    for (uint32_t i = 0; i + 1 < info->maxSubLayers; i++) {
        profilePresent[i] = u(&b, 1);
        levelPresent[i] = u(&b, 1);
    }
    if (info->maxSubLayers > 1)
        for (uint32_t i = info->maxSubLayers - 1; i < 8; i++)
            u(&b, 2);
    for (uint32_t i = 0; i + 1 < info->maxSubLayers; i++) {
        if (profilePresent[i]) {
            u(&b, 32);
            u(&b, 32);
            u(&b, 24);
        }
        if (levelPresent[i])
            u(&b, 8);
    }
    ue(&b);                                      /* sps_seq_parameter_set_id */
    info->chroma = ue(&b);
    if (info->chroma == 3)
        u(&b, 1);
    uint32_t width = ue(&b), height = ue(&b);
    uint32_t left = 0, right = 0, top = 0, bottom = 0;
    if (u(&b, 1)) {
        left = ue(&b);
        right = ue(&b);
        top = ue(&b);
        bottom = ue(&b);
    }
    info->lumaDepth = ue(&b) + 8;
    info->chromaDepth = ue(&b) + 8;
    Boolean ok = !b.overrun;
    free(b.bytes);
    if (!ok)
        return false;
    uint32_t subW = info->chroma == 1 || info->chroma == 2 ? 2 : 1, subH = info->chroma == 1 ? 2 : 1;
    info->dim.width = (int32_t)(width - subW * (left + right));
    info->dim.height = (int32_t)(height - subH * (top + bottom));
    return info->dim.width > 0 && info->dim.height > 0;
}

static CFDictionaryRef atomsExtension(CFStringRef atom, const uint8_t *bytes, size_t size, CFDictionaryRef more) {
    CFDataRef data = CFDataCreate(NULL, bytes, (CFIndex)size);
    CFDictionaryRef atoms = CFDictionaryCreate(NULL, (const void **)&atom, (const void **)&data, 1,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef ext = more != NULL ? CFDictionaryCreateMutableCopy(NULL, 0, more)
                                              : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                          &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(ext, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, atoms);
    CFRelease(data);
    CFRelease(atoms);
    return ext;
}

static void put16(uint8_t *p, size_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

OSStatus CMVideoFormatDescriptionCreateFromH264ParameterSets(CFAllocatorRef allocator, size_t count,
                                                             const uint8_t *const *sets, const size_t *sizes,
                                                             int headerLength, CMFormatDescriptionRef *out) {
    if (out == NULL || count < 2 || sets == NULL || sizes == NULL ||
        (headerLength != 1 && headerLength != 2 && headerLength != 4))
        return kCMFormatDescriptionError_InvalidParameter;
    size_t total = 7, spsCount = 0, ppsCount = 0, firstSPS = count;
    for (size_t i = 0; i < count; i++) {
        if (sets[i] == NULL || sizes[i] < 2 || sizes[i] > 0xFFFF)
            return kCMFormatDescriptionError_InvalidParameter;
        int type = sets[i][0] & 0x1F;
        if (type == 7) {
            spsCount++;
            if (firstSPS == count)
                firstSPS = i;
        } else if (type == 8)
            ppsCount++;
        else
            return kCMFormatDescriptionError_InvalidParameter;
        total += 2 + sizes[i];
    }
    CMVideoDimensions dim;
    H264Format format;
    if (spsCount == 0 || ppsCount == 0 || spsCount > 31 || sizes[firstSPS] < 4 ||
        !h264Dimensions(sets[firstSPS], sizes[firstSPS], &dim, &format))
        return kCMFormatDescriptionError_InvalidParameter;
    /* The high profiles' record ends with the chroma format and the bit
       depths (no SPS extensions). */
    if (format.high)
        total += 4;
    /* The AVC decoder configuration record. */
    uint8_t *avcC = malloc(total), *p = avcC;
    *p++ = 1;
    *p++ = sets[firstSPS][1];
    *p++ = sets[firstSPS][2];
    *p++ = sets[firstSPS][3];
    *p++ = (uint8_t)(0xFC | (headerLength - 1));
    *p++ = (uint8_t)(0xE0 | spsCount);
    for (size_t i = 0; i < count; i++)
        if ((sets[i][0] & 0x1F) == 7) {
            put16(p, sizes[i]);
            memcpy(p + 2, sets[i], sizes[i]);
            p += 2 + sizes[i];
        }
    *p++ = (uint8_t)ppsCount;
    for (size_t i = 0; i < count; i++)
        if ((sets[i][0] & 0x1F) == 8) {
            put16(p, sizes[i]);
            memcpy(p + 2, sets[i], sizes[i]);
            p += 2 + sizes[i];
        }
    if (format.high) {
        *p++ = (uint8_t)(0xFC | format.chroma);
        *p++ = (uint8_t)(0xF8 | (format.lumaDepth - 8));
        *p++ = (uint8_t)(0xF8 | (format.chromaDepth - 8));
        *p++ = 0;
    }
    CFDictionaryRef ext = atomsExtension(CFSTR("avcC"), avcC, total, NULL);
    free(avcC);
    OSStatus err = CMVideoFormatDescriptionCreate(allocator, kCMVideoCodecType_H264, dim.width, dim.height, ext, out);
    CFRelease(ext);
    return err;
}

static CFDataRef atom(CMFormatDescriptionRef d, CFStringRef name) {
    CFDictionaryRef atoms = d != NULL ? CFDictionaryGetValue(d->extensions,
                                                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms)
                                      : NULL;
    CFTypeRef v = atoms != NULL && CFGetTypeID(atoms) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(atoms, name) : NULL;
    return v != NULL && CFGetTypeID(v) == CFDataGetTypeID() ? v : NULL;
}

OSStatus CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef d, size_t index,
                                                            const uint8_t **setOut, size_t *sizeOut, size_t *countOut,
                                                            int *headerLengthOut) {
    CFDataRef data = atom(d, CFSTR("avcC"));
    if (data == NULL || CFDataGetLength(data) < 7)
        return kCMFormatDescriptionError_InvalidParameter;
    const uint8_t *p = CFDataGetBytePtr(data), *end = p + CFDataGetLength(data);
    if (headerLengthOut != NULL)
        *headerLengthOut = (p[4] & 3) + 1;
    size_t found = 0;
    const uint8_t *set = NULL;
    size_t setSize = 0;
    p += 5;
    for (int group = 0; group < 2 && p < end; group++) {
        size_t n = group == 0 ? (*p++ & 0x1F) : *p++;
        for (size_t i = 0; i < n; i++) {
            if (p + 2 > end || p + 2 + ((p[0] << 8) | p[1]) > end)
                return kCMFormatDescriptionError_InvalidParameter;
            size_t len = (size_t)((p[0] << 8) | p[1]);
            if (found == index) {
                set = p + 2;
                setSize = len;
            }
            found++;
            p += 2 + len;
        }
    }
    if (countOut != NULL)
        *countOut = found;
    if (setOut == NULL && sizeOut == NULL)
        return noErr;
    if (set == NULL)
        return kCMFormatDescriptionError_InvalidParameter;
    if (setOut != NULL)
        *setOut = set;
    if (sizeOut != NULL)
        *sizeOut = setSize;
    return noErr;
}

OSStatus CMVideoFormatDescriptionCreateFromHEVCParameterSets(CFAllocatorRef allocator, size_t count,
                                                             const uint8_t *const *sets, const size_t *sizes,
                                                             int headerLength, CFDictionaryRef extensions,
                                                             CMFormatDescriptionRef *out) {
    if (out == NULL || count < 3 || sets == NULL || sizes == NULL ||
        (headerLength != 1 && headerLength != 2 && headerLength != 4))
        return kCMFormatDescriptionError_InvalidParameter;
    /* The arrays, in this order: VPS, SPS, PPS, then SEI. */
    static const int types[] = { 32, 33, 34, 39, 40 };
    size_t perType[5] = { 0 }, total = 23, firstSPS = count;
    for (size_t i = 0; i < count; i++) {
        if (sets[i] == NULL || sizes[i] < 3 || sizes[i] > 0xFFFF)
            return kCMFormatDescriptionError_InvalidParameter;
        int type = (sets[i][0] >> 1) & 0x3F, k = -1;
        for (int t = 0; t < 5; t++)
            if (types[t] == type)
                k = t;
        if (k < 0)
            return kCMFormatDescriptionError_InvalidParameter;
        if (type == 33 && firstSPS == count)
            firstSPS = i;
        perType[k]++;
        total += 2 + sizes[i];
    }
    HEVCInfo info;
    if (perType[0] == 0 || perType[1] == 0 || perType[2] == 0 || !hevcInfo(sets[firstSPS], sizes[firstSPS], &info))
        return kCMFormatDescriptionError_InvalidParameter;
    int arrays = 0;
    for (int t = 0; t < 5; t++)
        if (perType[t] > 0) {
            arrays++;
            total += 3;
        }
    /* The HEVC decoder configuration record. */
    uint8_t *hvcC = malloc(total), *p = hvcC;
    *p++ = 1;
    memcpy(p, info.ptl, 12);
    p += 12;
    *p++ = 0xF0;                                 /* min_spatial_segmentation 0 */
    *p++ = 0x00;
    *p++ = 0xFC;                                 /* parallelismType 0 */
    *p++ = (uint8_t)(0xFC | info.chroma);
    *p++ = (uint8_t)(0xF8 | (info.lumaDepth - 8));
    *p++ = (uint8_t)(0xF8 | (info.chromaDepth - 8));
    *p++ = 0;                                    /* avgFrameRate */
    *p++ = 0;
    *p++ = (uint8_t)((info.maxSubLayers << 3) | (info.temporalNesting << 2) | (headerLength - 1));
    *p++ = (uint8_t)arrays;
    for (int t = 0; t < 5; t++) {
        if (perType[t] == 0)
            continue;
        *p++ = (uint8_t)(0x80 | types[t]);       /* array_completeness */
        put16(p, perType[t]);
        p += 2;
        for (size_t i = 0; i < count; i++)
            if (((sets[i][0] >> 1) & 0x3F) == types[t]) {
                put16(p, sizes[i]);
                memcpy(p + 2, sets[i], sizes[i]);
                p += 2 + sizes[i];
            }
    }
    CFDictionaryRef ext = atomsExtension(CFSTR("hvcC"), hvcC, total, extensions);
    free(hvcC);
    OSStatus err = CMVideoFormatDescriptionCreate(allocator, kCMVideoCodecType_HEVC, info.dim.width, info.dim.height,
                                                  ext, out);
    CFRelease(ext);
    return err;
}

OSStatus CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(CMFormatDescriptionRef d, size_t index,
                                                            const uint8_t **setOut, size_t *sizeOut, size_t *countOut,
                                                            int *headerLengthOut) {
    CFDataRef data = atom(d, CFSTR("hvcC"));
    if (data == NULL || CFDataGetLength(data) < 23)
        return kCMFormatDescriptionError_InvalidParameter;
    const uint8_t *p = CFDataGetBytePtr(data), *end = p + CFDataGetLength(data);
    if (headerLengthOut != NULL)
        *headerLengthOut = (p[21] & 3) + 1;
    size_t arrays = p[22], found = 0, setSize = 0;
    const uint8_t *set = NULL;
    p += 23;
    for (size_t a = 0; a < arrays; a++) {
        if (p + 3 > end)
            return kCMFormatDescriptionError_InvalidParameter;
        size_t n = (size_t)((p[1] << 8) | p[2]);
        p += 3;
        for (size_t i = 0; i < n; i++) {
            if (p + 2 > end || p + 2 + ((p[0] << 8) | p[1]) > end)
                return kCMFormatDescriptionError_InvalidParameter;
            size_t len = (size_t)((p[0] << 8) | p[1]);
            if (found == index) {
                set = p + 2;
                setSize = len;
            }
            found++;
            p += 2 + len;
        }
    }
    if (countOut != NULL)
        *countOut = found;
    if (setOut == NULL && sizeOut == NULL)
        return noErr;
    if (set == NULL)
        return kCMFormatDescriptionError_InvalidParameter;
    if (setOut != NULL)
        *setOut = set;
    if (sizeOut != NULL)
        *sizeOut = setSize;
    return noErr;
}
