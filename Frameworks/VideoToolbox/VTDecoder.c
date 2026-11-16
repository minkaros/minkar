/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * VTDecompressionSession: H.264, HEVC, VP9, AV1 and MPEG-4 decoded by
 * FFmpeg in the app's process, as on the reference platform. FFmpeg's
 * libraries (/usr/lib/media) are loaded with dlopen when a session is
 * first made, not linked.
 *
 * As the reference's decoder, each frame given is answered once through
 * the output callback, in decode order, while DecodeFrame runs: with its
 * image, or with none when it was not decoded or was not to be output.
 * The decoder runs with low delay (no reordering of its own): an app
 * puts frames in presentation order itself. Each image is a CVPixelBuffer
 * from a pool, in the pixel format the app asked for (an IOSurface when
 * its attributes ask for one), converted by swscale.
 */

#include "VTSession.h"
#include <CoreFoundation/CFRuntime.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>

#define kMediaLibraries "/usr/lib/media/"

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
    struct SwsContext *(*sws_getCachedContext)(struct SwsContext *, int, int, enum AVPixelFormat, int, int,
                                               enum AVPixelFormat, int, SwsFilter *, SwsFilter *, const double *);
    int (*sws_scale)(struct SwsContext *, const uint8_t *const[], const int[], int, int, uint8_t *const[], const int[]);
    void (*sws_freeContext)(struct SwsContext *);
} ff;
static int ffLoaded;
static pthread_once_t ffOnce = PTHREAD_ONCE_INIT;

static void loadFFmpeg(void) {
    void *avcodec = dlopen(kMediaLibraries "libavcodec.dylib", RTLD_LAZY | RTLD_LOCAL);
    void *avutil = dlopen(kMediaLibraries "libavutil.dylib", RTLD_LAZY | RTLD_LOCAL);
    void *sws = dlopen(kMediaLibraries "libswscale.dylib", RTLD_LAZY | RTLD_LOCAL);

    ffLoaded = -1;
    if (avcodec == NULL || avutil == NULL || sws == NULL)
        return;
#define FIND(lib, field, name)                            \
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
    FIND(sws, sws_getCachedContext, "sws_getCachedContext")
    FIND(sws, sws_scale, "sws_scale")
    FIND(sws, sws_freeContext, "sws_freeContext")
#undef FIND
    ffLoaded = 1;
}

static Boolean haveFFmpeg(void) {
    pthread_once(&ffOnce, loadFFmpeg);
    return ffLoaded == 1;
}

/* The decoder and the sample description atom (its extradata) of a codec. */
static enum AVCodecID codecFor(CMVideoCodecType type, CFStringRef *atom) {
    *atom = NULL;
    switch (type) {
    case kCMVideoCodecType_H264:
        *atom = CFSTR("avcC");
        return AV_CODEC_ID_H264;
    case kCMVideoCodecType_HEVC:
    case 'hev1':
        *atom = CFSTR("hvcC");
        return AV_CODEC_ID_HEVC;
    case 'vp09':
        *atom = CFSTR("vpcC");
        return AV_CODEC_ID_VP9;
    case 'av01':
        *atom = CFSTR("av1C");
        return AV_CODEC_ID_AV1;
    case kCMVideoCodecType_MPEG4Video:
        *atom = CFSTR("esds");
        return AV_CODEC_ID_MPEG4;
    default:
        return AV_CODEC_ID_NONE;
    }
}

static enum AVPixelFormat pixelFormatFor(OSType f) {
    switch (f) {
    case kCVPixelFormatType_420YpCbCr8Planar:
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
        return AV_PIX_FMT_YUV420P;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return AV_PIX_FMT_NV12;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        return AV_PIX_FMT_P010LE;
    case kCVPixelFormatType_422YpCbCr8:
        return AV_PIX_FMT_UYVY422;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
        return AV_PIX_FMT_YUYV422;
    case kCVPixelFormatType_32BGRA:
        return AV_PIX_FMT_BGRA;
    case kCVPixelFormatType_32ARGB:
        return AV_PIX_FMT_ARGB;
    default:
        return AV_PIX_FMT_NONE;
    }
}

/* A frame given and not yet answered: its reference and times. */
typedef struct {
    void *refCon;
    CMTime pts, duration;
    Boolean output, used;
    int64_t key;
} PendingFrame;

#define kMaxPending 64

struct OpaqueVTDecompressionSession {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CMVideoFormatDescriptionRef format;
    VTDecompressionOutputCallbackRecord callback;
    CFDictionaryRef attributes;
    OSType pixelFormat;
    CVPixelBufferPoolRef pool;
    int32_t poolWidth, poolHeight;
    AVCodecContext *ctx;
    AVPacket *packet;
    AVFrame *frame;
    struct SwsContext *sws;
    PendingFrame pending[kMaxPending];
    int64_t nextKey;
    Boolean invalid;
};

static void releaseDecoder(VTDecompressionSessionRef s) {
    if (s->ctx != NULL)
        ff.free_context(&s->ctx);
    if (s->packet != NULL)
        ff.packet_free(&s->packet);
    if (s->frame != NULL)
        ff.frame_free(&s->frame);
    if (s->sws != NULL)
        ff.sws_freeContext(s->sws);
    s->sws = NULL;
    if (s->pool != NULL)
        CVPixelBufferPoolRelease(s->pool);
    s->pool = NULL;
}

static void finalize(CFTypeRef cf) {
    VTDecompressionSessionRef s = (VTDecompressionSessionRef)cf;

    releaseDecoder(s);
    if (s->format != NULL)
        CFRelease(s->format);
    if (s->attributes != NULL)
        CFRelease(s->attributes);
    pthread_mutex_destroy(&s->lock);
}

static const CFRuntimeClass sessionClass = { 0, "VTDecompressionSession", NULL, NULL, finalize, NULL, NULL, NULL, NULL };

CFTypeID VTDecompressionSessionGetTypeID(void) {
    static CFTypeID typeID;
    static dispatch_once_t once;

    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&sessionClass); });
    return typeID;
}

static OSType requestedPixelFormat(CFDictionaryRef attributes) {
    CFTypeRef v = attributes != NULL ? CFDictionaryGetValue(attributes, kCVPixelBufferPixelFormatTypeKey) : NULL;
    SInt32 f = 0;

    if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID() && CFArrayGetCount(v) > 0)
        v = CFArrayGetValueAtIndex(v, 0);     /* the first the app takes */
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID() && CFNumberGetValue(v, kCFNumberSInt32Type, &f) &&
        pixelFormatFor((OSType)f) != AV_PIX_FMT_NONE)
        return (OSType)f;
    return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
}

static OSStatus openDecoder(VTDecompressionSessionRef s) {
    CFStringRef atomKey;
    enum AVCodecID id = codecFor(CMFormatDescriptionGetMediaSubType(s->format), &atomKey);
    const AVCodec *codec = id != AV_CODEC_ID_NONE ? ff.find_decoder(id) : NULL;
    CMVideoDimensions size = CMVideoFormatDescriptionGetDimensions(s->format);

    if (codec == NULL)
        return kVTCouldNotFindVideoDecoderErr;
    s->ctx = ff.alloc_context3(codec);
    s->packet = ff.packet_alloc();
    s->frame = ff.frame_alloc();
    if (s->ctx == NULL || s->packet == NULL || s->frame == NULL)
        return kVTAllocationFailedErr;
    s->ctx->width = size.width;
    s->ctx->height = size.height;
    /* One frame out for each in, in decode order: no reordering here and
       no frame threads (their delay), slices on threads instead. */
    s->ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    s->ctx->thread_type = FF_THREAD_SLICE;
    s->ctx->thread_count = 0;

    CFDictionaryRef atoms = CMFormatDescriptionGetExtension(s->format,
                                                            kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
    CFTypeRef atom = atoms != NULL && atomKey != NULL && CFGetTypeID(atoms) == CFDictionaryGetTypeID()
                         ? CFDictionaryGetValue(atoms, atomKey) : NULL;
    if (atom != NULL && CFGetTypeID(atom) == CFArrayGetTypeID() && CFArrayGetCount(atom) > 0)
        atom = CFArrayGetValueAtIndex(atom, 0);
    if (atom != NULL && CFGetTypeID(atom) == CFDataGetTypeID() && CFDataGetLength(atom) > 0 && id != AV_CODEC_ID_VP9) {
        /* avcC, hvcC and av1C are FFmpeg's extradata as they are (vpcC
           carries nothing its decoder needs). */
        CFIndex n = CFDataGetLength(atom);
        s->ctx->extradata = ff.mallocz((size_t)n + AV_INPUT_BUFFER_PADDING_SIZE);
        if (s->ctx->extradata == NULL)
            return kVTAllocationFailedErr;
        memcpy(s->ctx->extradata, CFDataGetBytePtr(atom), (size_t)n);
        s->ctx->extradata_size = (int)n;
    }
    if (ff.open2(s->ctx, codec, NULL) < 0)
        return kVTVideoDecoderBadDataErr;
    return noErr;
}

OSStatus VTDecompressionSessionCreate(CFAllocatorRef allocator, CMVideoFormatDescriptionRef format,
    CFDictionaryRef decoderSpecification, CFDictionaryRef imageBufferAttributes,
    const VTDecompressionOutputCallbackRecord *outputCallback, VTDecompressionSessionRef *sessionOut) {
    VTDecompressionSessionRef s;
    CFStringRef atom;
    OSStatus status;

    if (sessionOut == NULL || format == NULL || outputCallback == NULL ||
        outputCallback->decompressionOutputCallback == NULL)
        return kVTParameterErr;
    *sessionOut = NULL;
    if (codecFor(CMFormatDescriptionGetMediaSubType(format), &atom) == AV_CODEC_ID_NONE || !haveFFmpeg())
        return kVTCouldNotFindVideoDecoderErr;
    s = (VTDecompressionSessionRef)_CFRuntimeCreateInstance(allocator, VTDecompressionSessionGetTypeID(),
                                                            sizeof(*s) - sizeof(CFRuntimeBase), NULL);
    if (s == NULL)
        return kVTAllocationFailedErr;
    pthread_mutex_init(&s->lock, NULL);
    s->format = (CMVideoFormatDescriptionRef)CFRetain(format);
    s->callback = *outputCallback;
    s->attributes = imageBufferAttributes != NULL ? CFRetain(imageBufferAttributes) : NULL;
    s->pixelFormat = requestedPixelFormat(imageBufferAttributes);
    status = openDecoder(s);
    if (status != noErr) {
        CFRelease(s);
        return status;
    }
    *sessionOut = s;
    return noErr;
}

/* A pool of the app's attributes at this size and pixel format. */
static CVPixelBufferPoolRef poolFor(VTDecompressionSessionRef s, int32_t w, int32_t h) {
    if (s->pool != NULL && s->poolWidth == w && s->poolHeight == h)
        return s->pool;
    if (s->pool != NULL)
        CVPixelBufferPoolRelease(s->pool);
    CFMutableDictionaryRef a = s->attributes != NULL
        ? CFDictionaryCreateMutableCopy(NULL, 0, s->attributes)
        : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    SInt32 f = (SInt32)s->pixelFormat;
    CFNumberRef width = CFNumberCreate(NULL, kCFNumberSInt32Type, &w);
    CFNumberRef height = CFNumberCreate(NULL, kCFNumberSInt32Type, &h);
    CFNumberRef format = CFNumberCreate(NULL, kCFNumberSInt32Type, &f);
    CFDictionarySetValue(a, kCVPixelBufferWidthKey, width);
    CFDictionarySetValue(a, kCVPixelBufferHeightKey, height);
    CFDictionarySetValue(a, kCVPixelBufferPixelFormatTypeKey, format);
    CFRelease(width);
    CFRelease(height);
    CFRelease(format);
    s->pool = NULL;
    CVPixelBufferPoolCreate(NULL, NULL, a, &s->pool);
    CFRelease(a);
    s->poolWidth = w;
    s->poolHeight = h;
    return s->pool;
}

/* A decoded frame in a pixel buffer of the app's format. */
static CVPixelBufferRef imageOf(VTDecompressionSessionRef s, const AVFrame *f) {
    CVPixelBufferPoolRef pool = poolFor(s, f->width, f->height);
    CVPixelBufferRef image = NULL;
    uint8_t *planes[4] = { NULL };
    int strides[4] = { 0 };

    if (pool == NULL || CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &image) != kCVReturnSuccess || image == NULL)
        return NULL;
    s->sws = ff.sws_getCachedContext(s->sws, f->width, f->height, f->format, f->width, f->height,
                                     pixelFormatFor(s->pixelFormat), SWS_BILINEAR, NULL, NULL, NULL);
    if (s->sws == NULL || CVPixelBufferLockBaseAddress(image, 0) != kCVReturnSuccess) {
        CVPixelBufferRelease(image);
        return NULL;
    }
    if (CVPixelBufferIsPlanar(image)) {
        for (size_t p = 0; p < CVPixelBufferGetPlaneCount(image) && p < 4; p++) {
            planes[p] = CVPixelBufferGetBaseAddressOfPlane(image, p);
            strides[p] = (int)CVPixelBufferGetBytesPerRowOfPlane(image, p);
        }
    } else {
        planes[0] = CVPixelBufferGetBaseAddress(image);
        strides[0] = (int)CVPixelBufferGetBytesPerRow(image);
    }
    ff.sws_scale(s->sws, (const uint8_t *const *)f->data, f->linesize, 0, f->height, planes, strides);
    CVPixelBufferUnlockBaseAddress(image, 0);
    return image;
}

static PendingFrame *pendingWithKey(VTDecompressionSessionRef s, int64_t key) {
    for (int i = 0; i < kMaxPending; i++)
        if (s->pending[i].used && s->pending[i].key == key)
            return &s->pending[i];
    return NULL;
}

/* The answer for a frame given: its image, or none. */
static void answer(VTDecompressionSessionRef s, PendingFrame *p, OSStatus status, CVImageBufferRef image) {
    PendingFrame done = *p;

    p->used = false;
    s->callback.decompressionOutputCallback(s->callback.decompressionOutputRefCon, done.refCon, status,
                                            image == NULL ? kVTDecodeInfo_FrameDropped : 0, image, done.pts,
                                            done.duration);
}

/* The frames the decoder has: each answered for the frame it came of. */
static void emitFrames(VTDecompressionSessionRef s) {
    while (ff.receive_frame(s->ctx, s->frame) == 0) {
        PendingFrame *p = pendingWithKey(s, (int64_t)(intptr_t)s->frame->opaque);
        if (p != NULL) {
            CVPixelBufferRef image = p->output ? imageOf(s, s->frame) : NULL;
            answer(s, p, image != NULL || !p->output ? noErr : kVTVideoDecoderMalfunctionErr, image);
            if (image != NULL)
                CVPixelBufferRelease(image);
        }
        ff.frame_unref(s->frame);
    }
}

OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef s, CMSampleBufferRef sample,
    VTDecodeFrameFlags flags, void *refCon, VTDecodeInfoFlags *infoFlagsOut) {
    CMBlockBufferRef data;
    size_t length;
    PendingFrame *p = NULL;

    if (infoFlagsOut != NULL)
        *infoFlagsOut = 0;
    if (s == NULL || s->invalid || s->ctx == NULL)
        return kVTInvalidSessionErr;
    if (sample == NULL || (data = CMSampleBufferGetDataBuffer(sample)) == NULL ||
        (length = CMBlockBufferGetDataLength(data)) == 0)
        return kVTParameterErr;
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < kMaxPending && p == NULL; i++)
        if (!s->pending[i].used)
            p = &s->pending[i];
    if (p == NULL) {
        /* The oldest unanswered: the decoder kept them; answered empty. */
        p = &s->pending[0];
        for (int i = 1; i < kMaxPending; i++)
            if (s->pending[i].key < p->key)
                p = &s->pending[i];
        answer(s, p, noErr, NULL);
    }
    p->used = true;
    p->refCon = refCon;
    p->pts = CMSampleBufferGetPresentationTimeStamp(sample);
    p->duration = CMSampleBufferGetDuration(sample);
    p->output = !(flags & kVTDecodeFrame_DoNotOutputFrame);
    p->key = ++s->nextKey;

    if (ff.new_packet(s->packet, (int)length) < 0 ||
        CMBlockBufferCopyDataBytes(data, 0, length, s->packet->data) != kCMBlockBufferNoErr) {
        ff.packet_unref(s->packet);
        answer(s, p, kVTVideoDecoderBadDataErr, NULL);
        pthread_mutex_unlock(&s->lock);
        return noErr;
    }
    s->packet->opaque = (void *)(intptr_t)p->key;
    s->ctx->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
    int sent = ff.send_packet(s->ctx, s->packet);
    ff.packet_unref(s->packet);
    if (sent == AVERROR(EAGAIN)) {
        emitFrames(s);
        ff.new_packet(s->packet, (int)length);
        CMBlockBufferCopyDataBytes(data, 0, length, s->packet->data);
        s->packet->opaque = (void *)(intptr_t)p->key;
        sent = ff.send_packet(s->ctx, s->packet);
        ff.packet_unref(s->packet);
    }
    emitFrames(s);
    /* Not decoded (broken, or only a parameter set): answered empty, as
       the reference does, so the app is not left waiting for it. */
    if (sent < 0 && p->used && p->key == s->nextKey)
        answer(s, p, kVTVideoDecoderBadDataErr, NULL);
    else if (p->used && p->key == s->nextKey)
        answer(s, p, noErr, NULL);
    pthread_mutex_unlock(&s->lock);
    return noErr;
}

/* Every frame given has been answered when DecodeFrame returns. */
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef s) {
    if (s == NULL || s->invalid)
        return kVTInvalidSessionErr;
    return noErr;
}

OSStatus VTDecompressionSessionFinishDelayedFrames(VTDecompressionSessionRef s) {
    return VTDecompressionSessionWaitForAsynchronousFrames(s);
}

Boolean VTDecompressionSessionCanAcceptFormatDescription(VTDecompressionSessionRef s,
                                                         CMFormatDescriptionRef format) {
    CFStringRef a, b;

    if (s == NULL || format == NULL || s->invalid)
        return false;
    return codecFor(CMFormatDescriptionGetMediaSubType(format), &a) ==
               codecFor(CMFormatDescriptionGetMediaSubType(s->format), &b) &&
           CFEqual(format, s->format);
}

void VTDecompressionSessionInvalidate(VTDecompressionSessionRef s) {
    if (s == NULL)
        return;
    pthread_mutex_lock(&s->lock);
    /* What the decoder still holds is answered empty. */
    for (int i = 0; i < kMaxPending; i++)
        if (s->pending[i].used)
            answer(s, &s->pending[i], noErr, NULL);
    s->invalid = true;
    releaseDecoder(s);
    pthread_mutex_unlock(&s->lock);
}

Boolean _VTIsDecompressionSession(CFTypeRef cf) {
    return cf != NULL && CFGetTypeID(cf) == VTDecompressionSessionGetTypeID();
}
