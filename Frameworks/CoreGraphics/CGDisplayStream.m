/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * CGDisplayStream (WindowServer plan WS5): a display's frames in
 * IOSurfaces. The stream makes its surfaces (kCGDisplayStreamQueueDepth of
 * them, 3 by default) and a Mach port; the window server copies each frame
 * that changed into a surface it finds unused (use count 0), adds a use,
 * and sends the port a message (SLSDisplayStreamFrame). The stream calls
 * the handler, on its queue or run loop, then takes its use back: a
 * handler that keeps the surface adds a use of its own, as on the
 * reference platform.
 */

#import <CoreFoundation/CFRuntime.h>
#import <CoreFoundation/CFMachPort.h>
#import <CoreGraphics/CGDirectDisplay.h>
#import <CoreGraphics/CGGeometry.h>
#include <Block.h>
#include <IOSurface/IOSurfaceRef.h>
#include <SkyLight/SkyLight.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>

#define kMaxQueueDepth	8

struct CGDisplayStream {
    CFRuntimeBase base;
    CGDirectDisplayID display;
    SLSDisplayStreamOptions options;
    CGDisplayStreamFrameAvailableHandler handler;
    dispatch_queue_t queue;             /* NULL: the run loop source */
    dispatch_source_t source;
    CFMachPortRef machPort;
    CFRunLoopSourceRef runLoopSource;
    mach_port_t port;
    IOSurfaceRef surfaces[kMaxQueueDepth];
    uint32_t count;
    uint32_t streamID;                  /* 0 while stopped */
};

struct CGDisplayStreamUpdate {
    CFRuntimeBase base;
    size_t count;
    CGRect *rects;
    CGRect reduced;                     /* the box around them */
    size_t dropCount;
};

static CFTypeID streamTypeID, updateTypeID;

static void finalizeStream(CFTypeRef cf);
static void finalizeUpdate(CFTypeRef cf) {
    free(((struct CGDisplayStreamUpdate *)(uintptr_t)cf)->rects);
}

static const CFRuntimeClass streamClass = {
    0, "CGDisplayStream", NULL, NULL, finalizeStream, NULL, NULL, NULL, NULL
};
static const CFRuntimeClass updateClass = {
    0, "CGDisplayStreamUpdate", NULL, NULL, finalizeUpdate, NULL, NULL, NULL, NULL
};

static void registerClasses(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        streamTypeID = _CFRuntimeRegisterClass(&streamClass);
        updateTypeID = _CFRuntimeRegisterClass(&updateClass);
    });
}

CFTypeID CGDisplayStreamGetTypeID(void) {
    registerClasses();
    return streamTypeID;
}

CFTypeID CGDisplayStreamUpdateGetTypeID(void) {
    registerClasses();
    return updateTypeID;
}

static CGDisplayStreamUpdateRef createUpdate(const CGRect *rects, size_t count) {
    struct CGDisplayStreamUpdate *u = (struct CGDisplayStreamUpdate *)(uintptr_t)
        _CFRuntimeCreateInstance(kCFAllocatorDefault, CGDisplayStreamUpdateGetTypeID(),
        sizeof(*u) - sizeof(CFRuntimeBase), NULL);
    if(u == NULL)
        return NULL;
    u->count = count;
    u->rects = count ? malloc(count * sizeof(CGRect)) : NULL;
    u->reduced = CGRectNull;
    u->dropCount = 0;
    for(size_t i = 0; i < count; i++) {
        u->rects[i] = rects[i];
        u->reduced = CGRectUnion(u->reduced, rects[i]);
    }
    return u;
}

/* One message from the window server: a frame, or the end. */
static void deliver(struct CGDisplayStream *s, const SLSDisplayStreamFrame *m) {
    if(m->header.msgh_id != kSLSDisplayStreamMessageID || m->stream != s->streamID)
        return;
    if(m->status == kSLSDisplayStreamStopped) {
        s->streamID = 0;
        s->handler(kCGDisplayStreamFrameStatusStopped, m->time, NULL, NULL);
        return;
    }
    IOSurfaceRef surface = NULL;
    for(uint32_t i = 0; i < s->count; i++)
        if(IOSurfaceGetID(s->surfaces[i]) == m->surface)
            surface = s->surfaces[i];
    if(surface == NULL)
        return;
    CGRect rects[kSLSMaxStreamRects];
    uint32_t n = m->count < kSLSMaxStreamRects ? m->count : kSLSMaxStreamRects;
    for(uint32_t i = 0; i < n; i++)
        rects[i] = CGRectMake(m->rects[i].x, m->rects[i].y, m->rects[i].width,
            m->rects[i].height);
    CGDisplayStreamUpdateRef update = createUpdate(rects, n);
    s->handler(kCGDisplayStreamFrameStatusFrameComplete, m->time, surface, update);
    if(update != NULL)
        CFRelease(update);
    /* The server's use: the surface is free for another frame unless the
       handler kept a use of its own. */
    IOSurfaceDecrementUseCount(surface);
}

typedef struct {
    SLSDisplayStreamFrame frame;
    mach_msg_max_trailer_t trailer;
} FrameBuffer;

static void receiveAll(struct CGDisplayStream *s) {
    FrameBuffer buffer;

    for(;;) {
        memset(&buffer, 0, sizeof(buffer));
        if(mach_msg(&buffer.frame.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(buffer),
            s->port, 0, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
            return;
        deliver(s, &buffer.frame);
    }
}

static void machPortCallBack(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    if(msg != NULL && size >= (CFIndex)sizeof(SLSDisplayStreamFrame))
        deliver((struct CGDisplayStream *)info, (const SLSDisplayStreamFrame *)msg);
}

static int intProperty(CFDictionaryRef properties, CFStringRef key, int fallback) {
    CFTypeRef v = properties ? CFDictionaryGetValue(properties, key) : NULL;
    int value = fallback;
    if(v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef)v, kCFNumberIntType, &value);
    return value;
}

static struct CGDisplayStream *createStream(CGDirectDisplayID display, size_t outputWidth,
    size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties,
    dispatch_queue_t queue, CGDisplayStreamFrameAvailableHandler handler) {
    /* BGRA only: the window server composites 8-bit RGB. */
    if(handler == NULL || outputWidth == 0 || outputHeight == 0 || pixelFormat != 'BGRA')
        return NULL;
    struct CGDisplayStream *s = (struct CGDisplayStream *)(uintptr_t)_CFRuntimeCreateInstance(
        kCFAllocatorDefault, CGDisplayStreamGetTypeID(), sizeof(*s) - sizeof(CFRuntimeBase), NULL);
    if(s == NULL)
        return NULL;
    memset((char *)s + sizeof(CFRuntimeBase), 0, sizeof(*s) - sizeof(CFRuntimeBase));
    s->display = display;
    s->handler = Block_copy(handler);

    SLSDisplayStreamOptions *o = &s->options;
    o->outputWidth = (uint32_t)outputWidth;
    o->outputHeight = (uint32_t)outputHeight;
    o->showCursor = 1;
    CFTypeRef v;
    if(properties != NULL) {
        CGRect source;
        v = CFDictionaryGetValue(properties, kCGDisplayStreamSourceRect);
        if(v != NULL && CFGetTypeID(v) == CFDictionaryGetTypeID() &&
           CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)v, &source)) {
            o->sourceX = (int32_t)source.origin.x;
            o->sourceY = (int32_t)source.origin.y;
            o->sourceWidth = (uint32_t)source.size.width;
            o->sourceHeight = (uint32_t)source.size.height;
        }
        v = CFDictionaryGetValue(properties, kCGDisplayStreamMinimumFrameTime);
        double seconds = 0;
        if(v != NULL && CFGetTypeID(v) == CFNumberGetTypeID() &&
           CFNumberGetValue((CFNumberRef)v, kCFNumberDoubleType, &seconds) && seconds > 0)
            o->minFrameMicros = (uint32_t)(seconds * 1e6);
        v = CFDictionaryGetValue(properties, kCGDisplayStreamShowCursor);
        if(v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID())
            o->showCursor = CFBooleanGetValue((CFBooleanRef)v) ? 1 : 0;
    }
    int depth = intProperty(properties, kCGDisplayStreamQueueDepth, 3);
    if(depth < 1)
        depth = 1;
    if(depth > kMaxQueueDepth)
        depth = kMaxQueueDepth;

    int w = (int)outputWidth, h = (int)outputHeight, bpe = 4, format = 'BGRA';
    CFNumberRef nw = CFNumberCreate(NULL, kCFNumberIntType, &w);
    CFNumberRef nh = CFNumberCreate(NULL, kCFNumberIntType, &h);
    CFNumberRef nb = CFNumberCreate(NULL, kCFNumberIntType, &bpe);
    CFNumberRef nf = CFNumberCreate(NULL, kCFNumberIntType, &format);
    const void *keys[] = { kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement,
        kIOSurfacePixelFormat };
    const void *values[] = { nw, nh, nb, nf };
    CFDictionaryRef surfaceProperties = CFDictionaryCreate(NULL, keys, values, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(nw);
    CFRelease(nh);
    CFRelease(nb);
    CFRelease(nf);
    for(int i = 0; i < depth; i++) {
        s->surfaces[i] = IOSurfaceCreate(surfaceProperties);
        if(s->surfaces[i] == NULL)
            break;
        s->count++;
    }
    CFRelease(surfaceProperties);

    if(s->count != (uint32_t)depth ||
       mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->port) != KERN_SUCCESS) {
        CFRelease(s);
        return NULL;
    }
    if(queue != NULL) {
        s->queue = queue;
        dispatch_retain(queue);
        s->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, s->port, 0, queue);
        dispatch_source_set_event_handler(s->source, ^{ receiveAll(s); });
        dispatch_resume(s->source);
    } else {
        CFMachPortContext context = { 0, s, NULL, NULL, NULL };
        s->machPort = CFMachPortCreateWithPort(NULL, s->port, machPortCallBack, &context, NULL);
        if(s->machPort != NULL)
            s->runLoopSource = CFMachPortCreateRunLoopSource(NULL, s->machPort, 0);
    }
    return s;
}

static void finalizeStream(CFTypeRef cf) {
    struct CGDisplayStream *s = (struct CGDisplayStream *)(uintptr_t)cf;

    if(s->streamID != 0)
        SLSDisplayStreamStop(s->streamID);
    if(s->source != NULL) {
        dispatch_source_cancel(s->source);
        dispatch_release(s->source);
    }
    if(s->queue != NULL)
        dispatch_release(s->queue);
    if(s->runLoopSource != NULL) {
        CFRunLoopSourceInvalidate(s->runLoopSource);
        CFRelease(s->runLoopSource);
    }
    if(s->machPort != NULL) {
        CFMachPortInvalidate(s->machPort);
        CFRelease(s->machPort);
    }
    if(s->port != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_RECEIVE, -1);
    for(uint32_t i = 0; i < s->count; i++)
        CFRelease(s->surfaces[i]);
    if(s->handler != NULL)
        Block_release(s->handler);
}

CGDisplayStreamRef CGDisplayStreamCreate(CGDirectDisplayID display, size_t outputWidth,
    size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties,
    CGDisplayStreamFrameAvailableHandler handler) {
    return createStream(display, outputWidth, outputHeight, pixelFormat, properties, NULL,
        handler);
}

CGDisplayStreamRef CGDisplayStreamCreateWithDispatchQueue(CGDirectDisplayID display,
    size_t outputWidth, size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties,
    dispatch_queue_t queue, CGDisplayStreamFrameAvailableHandler handler) {
    if(queue == NULL)
        return NULL;
    return createStream(display, outputWidth, outputHeight, pixelFormat, properties, queue,
        handler);
}

CGError CGDisplayStreamStart(CGDisplayStreamRef stream) {
    struct CGDisplayStream *s = (struct CGDisplayStream *)stream;
    uint32_t ids[kMaxQueueDepth];

    if(s == NULL)
        return kCGErrorIllegalArgument;
    if(s->streamID != 0)
        return kCGErrorSuccess;
    for(uint32_t i = 0; i < s->count; i++)
        ids[i] = IOSurfaceGetID(s->surfaces[i]);
    return SLSDisplayStreamStart(s->display, ids, s->count, &s->options, s->port,
        &s->streamID);
}

CGError CGDisplayStreamStop(CGDisplayStreamRef stream) {
    struct CGDisplayStream *s = (struct CGDisplayStream *)stream;

    if(s == NULL)
        return kCGErrorIllegalArgument;
    if(s->streamID == 0)
        return kCGErrorSuccess;
    /* The handler hears kCGDisplayStreamFrameStatusStopped from the server. */
    return SLSDisplayStreamStop(s->streamID);
}

CFRunLoopSourceRef CGDisplayStreamGetRunLoopSource(CGDisplayStreamRef stream) {
    return stream != NULL ? ((struct CGDisplayStream *)stream)->runLoopSource : NULL;
}

const CGRect *CGDisplayStreamUpdateGetRects(CGDisplayStreamUpdateRef update,
    CGDisplayStreamUpdateRectType rectType, size_t *rectCount) {
    struct CGDisplayStreamUpdate *u = (struct CGDisplayStreamUpdate *)update;

    if(rectCount != NULL)
        *rectCount = 0;
    if(u == NULL)
        return NULL;
    switch(rectType) {
    case kCGDisplayStreamUpdateRefreshedRects:
    case kCGDisplayStreamUpdateDirtyRects:
        if(rectCount != NULL)
            *rectCount = u->count;
        return u->rects;
    case kCGDisplayStreamUpdateReducedDirtyRects:
        if(u->count == 0)
            return NULL;
        if(rectCount != NULL)
            *rectCount = 1;
        return &u->reduced;
    default:    /* moved rects: the server sends none */
        return NULL;
    }
}

CGDisplayStreamUpdateRef CGDisplayStreamUpdateCreateMergedUpdate(
    CGDisplayStreamUpdateRef first, CGDisplayStreamUpdateRef second) {
    struct CGDisplayStreamUpdate *a = (struct CGDisplayStreamUpdate *)first;
    struct CGDisplayStreamUpdate *b = (struct CGDisplayStreamUpdate *)second;
    size_t na = a ? a->count : 0, nb = b ? b->count : 0;
    CGRect *all = malloc((na + nb + 1) * sizeof(CGRect));

    if(all == NULL)
        return NULL;
    if(na)
        memcpy(all, a->rects, na * sizeof(CGRect));
    if(nb)
        memcpy(all + na, b->rects, nb * sizeof(CGRect));
    CGDisplayStreamUpdateRef merged = createUpdate(all, na + nb);
    free(all);
    if(merged != NULL)
        ((struct CGDisplayStreamUpdate *)merged)->dropCount =
            (a ? a->dropCount : 0) + (b ? b->dropCount : 0);
    return merged;
}

void CGDisplayStreamUpdateGetMovedRectsDelta(CGDisplayStreamUpdateRef update, CGFloat *dx,
    CGFloat *dy) {
    if(dx != NULL)
        *dx = 0;
    if(dy != NULL)
        *dy = 0;
}

size_t CGDisplayStreamUpdateGetDropCount(CGDisplayStreamUpdateRef update) {
    return update != NULL ? ((struct CGDisplayStreamUpdate *)update)->dropCount : 0;
}
