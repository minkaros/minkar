/*
 * Copyright (C) 2024-2026 Zoe Knox <zoe@pixin.net>
 * Modifications Copyright (c) 2026 The Minkar contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#import <mach/mach.h>
#import <sys/types.h>
#import <sys/mman.h>
#import <CoreGraphics/CGDirectDisplay.h>
#import <CoreGraphics/CGError.h>
#import <Onyx2D/O2Context_builtin.h>
#import <CoreFoundation/CFMachPort.h>
#include <SkyLight/SkyLight.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#import <CoreGraphics/CGBitmapContext.h>
#import <CoreGraphics/CGWindow.h>

const CFStringRef kCGDisplayStreamSourceRect = CFSTR("kCGDisplayStreamSourceRect");
const CFStringRef kCGDisplayStreamDestinationRect = CFSTR("kCGDisplayStreamDestinationRect");
const CFStringRef kCGDisplayStreamPreserveAspectRatio = CFSTR("kCGDisplayStreamPreserveAspectRatio");
const CFStringRef kCGDisplayStreamColorSpace = CFSTR("kCGDisplayStreamColorSpace");
const CFStringRef kCGDisplayStreamMinimumFrameTime = CFSTR("kCGDisplayStreamMinimumFrameTime");
const CFStringRef kCGDisplayStreamShowCursor = CFSTR("kCGDisplayStreamShowCursor");
const CFStringRef kCGDisplayStreamQueueDepth = CFSTR("kCGDisplayStreamQueueDepth");
const CFStringRef kCGDisplayStreamYCbCrMatrix = CFSTR("kCGDisplayStreamYCbCrMatrix");

const CFStringRef kCGDisplayStreamYCbCrMatrix_ITU_R_709_2 = CFSTR("kCGDisplayStreamYCbCrMatrix_ITU_R_709_2");
const CFStringRef kCGDisplayStreamYCbCrMatrix_ITU_R_601_4 = CFSTR("kCGDisplayStreamYCbCrMatrix_ITU_R_601_4");
const CFStringRef kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995 = CFSTR("kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995");

/* A configuration: the window server's, and the fade around it. */
struct _CGDisplayConfig {
    SLSDisplayConfigRef sls;
    CGDisplayFadeInterval fadeOut, fadeIn;
    float fadeColor[3];
};

/* A display mode, as the window server describes it, reference counted. */
struct CGDisplayMode {
    int refcount;
    uint32_t modeID;
    uint32_t flags;
    uint32_t width, height;		/* points */
    uint32_t pixelWidth, pixelHeight;
    uint32_t depth;			/* bits per pixel */
    double refresh;			/* Hz, 0 when unknown */
};

static CGDisplayModeRef displayModeCreate(const SLSDisplayMode *m) {
    struct CGDisplayMode *mode = calloc(sizeof(struct CGDisplayMode), 1);

    if(mode == NULL)
        return NULL;
    mode->refcount = 1;
    mode->modeID = m->modeID;
    mode->flags = m->flags;
    mode->width = m->width;
    mode->height = m->height;
    mode->pixelWidth = m->pixelWidth;
    mode->pixelHeight = m->pixelHeight;
    mode->depth = m->depth;
    mode->refresh = m->refreshRate;
    return mode;
}

static const void *displayModeArrayRetain(CFAllocatorRef allocator, const void *value) {
    return CGDisplayModeRetain((CGDisplayModeRef)value);
}

static void displayModeArrayRelease(CFAllocatorRef allocator, const void *value) {
    CGDisplayModeRelease((CGDisplayModeRef)value);
}

/* An array of modes holds them. */
static const CFArrayCallBacks displayModeCallBacks = {
    0, displayModeArrayRetain, displayModeArrayRelease, NULL, NULL
};

static struct CGDisplayStream {
    uintptr_t cfisa;
};

static struct CGDisplayStreamUpdate {
    uintptr_t cfisa;
};

// dictionary of display contexts
static CFMutableDictionaryRef __displayContexts = NULL;
static const CFDictionaryKeyCallBacks __CGDispCtxKeyCallback = {0}; // all NULL - use defaults
static const CFDictionaryValueCallBacks __CGDispCtxValCallback = {0}; // same

static uint32_t CGDisplayStateFlags(CGDirectDisplayID display);

/*
 * Whether the window server answers (WindowServer plan: SkyLight's, the
 * only one since WS4b). Decided once.
 */
int _CGUsesSkyLight(void) {
    static dispatch_once_t once;
    static int uses;

    dispatch_once(&once, ^{
        /* The new server has the window server's service since WS4b. */
        uses = SLSMainConnectionID() != 0;
    });
    return uses;
}

// Finding displays
CGDirectDisplayID CGMainDisplayID(void) {
    return SLSMainDisplayID();
}

CGError CGGetOnlineDisplayList(uint32_t maxDisplays, CGDirectDisplayID *onlineDisplays, uint32_t *displayCount) {
    return SLSGetOnlineDisplayList(maxDisplays, onlineDisplays, displayCount);
}

CGError CGGetActiveDisplayList(uint32_t maxDisplays, CGDirectDisplayID *activeDisplays, uint32_t *displayCount) {
    return SLSGetActiveDisplayList(maxDisplays, activeDisplays, displayCount);
}

CGError CGGetDisplaysWithOpenGLDisplayMask(CGOpenGLDisplayMask mask, uint32_t maxDisplays, CGDirectDisplayID *displays, uint32_t *matchingDisplayCount) {
    CGDirectDisplayID active[32];
    uint32_t count = 0, n = 0;
    CGError err = SLSGetActiveDisplayList(32, active, &count);

    if(err != kCGErrorSuccess)
        return err;
    for(uint32_t i = 0; i < count; i++) {
        SLSDisplayInfo info;
        if(SLSGetDisplayInfo(active[i], &info) != kSLSErrorSuccess || !(info.openGLMask & mask))
            continue;
        if(displays != NULL && n < maxDisplays)
            displays[n] = active[i];
        n++;
    }
    if(matchingDisplayCount != NULL)
        *matchingDisplayCount = (displays != NULL && n > maxDisplays) ? maxDisplays : n;
    return kCGErrorSuccess;
}

CGError CGGetDisplaysWithPoint(CGPoint point, uint32_t maxDisplays, CGDirectDisplayID *displays, uint32_t *matchingDisplayCount) {
    return SLSGetDisplaysWithPoint(point.x, point.y, maxDisplays, displays, matchingDisplayCount);
}

CGError CGGetDisplaysWithRect(CGRect rect, uint32_t maxDisplays, CGDirectDisplayID *displays, uint32_t *matchingDisplayCount) {
    return SLSGetDisplaysWithRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
        maxDisplays, displays, matchingDisplayCount);
}

CGDirectDisplayID CGOpenGLDisplayMaskToDisplayID(CGOpenGLDisplayMask mask) {
    CGDirectDisplayID display = kCGNullDirectDisplay;
    uint32_t count = 0;

    /* The display of the mask's lowest bit. */
    mask &= -mask;
    if(mask == 0 || CGGetDisplaysWithOpenGLDisplayMask(mask, 1, &display, &count) != kCGErrorSuccess ||
        count == 0)
        return kCGNullDirectDisplay;
    return display;
}

CGOpenGLDisplayMask CGDisplayIDToOpenGLDisplayMask(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0;
    return info.openGLMask;
}

/* ---- Captured displays with the new server (WS4a) ---- */

/*
 * CGDisplayGetDrawingContext's context draws into memory of this process;
 * a timer copies it into the display's shielding window (its surface) at
 * the refresh rate, when it changed since the last copy.
 * SHORTCUT(WS5): the app should draw into the surface that is scanned out,
 * through IOSurface; until then a copy, and a frame behind; taken in WS4a.
 */
typedef struct {
    CGContextRef context;
    uint8_t *pixels, *flushed;     /* what is drawn; what was copied last */
    size_t width, height, rowBytes;
    SLSWindowID shield;
    dispatch_source_t timer;
} SLSDrawingContext;

static CFMutableDictionaryRef slsDrawingContexts;    /* display -> SLSDrawingContext */
static dispatch_queue_t slsDrawingQueue;

static void slsFlushDrawing(SLSDrawingContext *d) {
    size_t size = d->height * d->rowBytes;

    if(memcmp(d->pixels, d->flushed, size) == 0)
        return;
    memcpy(d->flushed, d->pixels, size);
    SLSIntRect all = { 0, 0, (int32_t)d->width, (int32_t)d->height };
    SLSWindowFlushPixels(d->shield, d->flushed, (uint32_t)d->rowBytes, &all, 1);
}

static void slsDropDrawingContext(CGDirectDisplayID display) {
    if(slsDrawingContexts == NULL)
        return;
    SLSDrawingContext *d = (SLSDrawingContext *)CFDictionaryGetValue(slsDrawingContexts,
        (const void *)(uintptr_t)display);
    if(d == NULL)
        return;
    CFDictionaryRemoveValue(slsDrawingContexts, (const void *)(uintptr_t)display);
    dispatch_source_cancel(d->timer);
    dispatch_sync(slsDrawingQueue, ^{});    /* no copy running */
    dispatch_release(d->timer);
    CGContextRelease(d->context);
    free(d->pixels);
    free(d->flushed);
    free(d);
}

static CGContextRef slsDrawingContext(CGDirectDisplayID display) {
    int32_t pid = 0;
    SLSWindowID shield = SLSShieldingWindowID(display);

    if(shield == 0 || SLSDisplayCapturedBy(display, &pid) != kSLSErrorSuccess || pid != getpid())
        return NULL;    /* not captured, or not by this process */
    if(slsDrawingContexts == NULL) {
        slsDrawingContexts = CFDictionaryCreateMutable(NULL, 8, NULL, NULL);
        slsDrawingQueue = dispatch_queue_create("org.minkar.CoreGraphics.capture", NULL);
    }
    SLSDrawingContext *d = (SLSDrawingContext *)CFDictionaryGetValue(slsDrawingContexts,
        (const void *)(uintptr_t)display);
    if(d != NULL)
        return d->context;

    CGRect bounds = CGDisplayBounds(display);
    d = calloc(1, sizeof(*d));
    d->width = (size_t)bounds.size.width;
    d->height = (size_t)bounds.size.height;
    d->rowBytes = d->width * 4;
    d->shield = shield;
    d->pixels = calloc(d->height, d->rowBytes);
    d->flushed = calloc(d->height, d->rowBytes);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    d->context = CGBitmapContextCreate(d->pixels, d->width, d->height, 8, d->rowBytes, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if(d->context == NULL || d->pixels == NULL || d->flushed == NULL) {
        if(d->context != NULL)
            CGContextRelease(d->context);
        free(d->pixels);
        free(d->flushed);
        free(d);
        return NULL;
    }
    /* Opaque black to start with, as the shielding window. */
    CGContextSetRGBFillColor(d->context, 0, 0, 0, 1);
    CGContextFillRect(d->context, CGRectMake(0, 0, d->width, d->height));
    memset(d->flushed, 0xff, d->height * d->rowBytes);    /* the first tick copies */
    double refresh = 60;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display);
    if(mode != NULL) {
        if(CGDisplayModeGetRefreshRate(mode) > 0)
            refresh = CGDisplayModeGetRefreshRate(mode);
        CGDisplayModeRelease(mode);
    }
    d->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, slsDrawingQueue);
    uint64_t interval = (uint64_t)(NSEC_PER_SEC / refresh);
    dispatch_source_set_timer(d->timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval,
        interval / 10);
    dispatch_source_set_event_handler(d->timer, ^{ slsFlushDrawing(d); });
    dispatch_resume(d->timer);
    CFDictionarySetValue(slsDrawingContexts, (const void *)(uintptr_t)display, d);
    return d->context;
}

static CGError slsRelease(CGDirectDisplayID display) {
    if(display == 0) {
        if(slsDrawingContexts != NULL) {
            CFIndex n = CFDictionaryGetCount(slsDrawingContexts);
            const void *keys[n > 0 ? n : 1];
            CFDictionaryGetKeysAndValues(slsDrawingContexts, keys, NULL);
            for(CFIndex i = 0; i < n; i++)
                slsDropDrawingContext((CGDirectDisplayID)(uintptr_t)keys[i]);
        }
    } else
        slsDropDrawingContext(display);
    return (CGError)SLSReleaseDisplay(display);
}

static void slsReleasePixels(void *info, const void *data, size_t size) {
    free((void *)data);
}

/* The screen's contents in a rect of a display: its on-screen windows,
 * composed over black, without the cursor. */
static CGImageRef slsDisplayImage(CGDirectDisplayID display, CGRect rect) {
    CGRect bounds = CGDisplayBounds(display);
    CGRect global = CGRectIntersection(CGRectOffset(rect, bounds.origin.x, bounds.origin.y), bounds);

    if(CGRectIsNull(global) || CGRectIsEmpty(global))
        return NULL;
    global = CGRectIntegral(global);
    size_t w = (size_t)global.size.width, h = (size_t)global.size.height;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    uint8_t *pixels = calloc(h, w * 4);
    CGContextRef context = CGBitmapContextCreate(pixels, w, h, 8, w * 4, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGImageRef image = NULL;

    if(context != NULL) {
        CGContextSetRGBFillColor(context, 0, 0, 0, 1);
        CGContextFillRect(context, CGRectMake(0, 0, w, h));
        CGImageRef windows = CGWindowListCreateImage(global, kCGWindowListOptionOnScreenOnly,
            kCGNullWindowID, kCGWindowImageDefault);
        if(windows != NULL) {
            CGContextDrawImage(context, CGRectMake(0, 0, w, h), windows);
            CGImageRelease(windows);
        }
        CGContextRelease(context);
        /* The image takes the pixels (a CF data object would reach
           Onyx2D's provider as an NSData, and CF's are not bridged). */
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels, h * w * 4,
            slsReleasePixels);
        image = CGImageCreate(w, h, 8, 32, w * 4, space,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, provider, NULL, false,
            kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
    } else
        free(pixels);
    CGColorSpaceRelease(space);
    return image;
}

// Capturing and Releasing Displays
CGError CGDisplayCapture(CGDirectDisplayID display) {
    return CGDisplayCaptureWithOptions(display, kCGCaptureNoOptions);
}

CGError CGDisplayCaptureWithOptions(CGDirectDisplayID display, CGCaptureOptions options) {
    return display == 0 ? kCGErrorIllegalArgument :
            (CGError)SLSCaptureDisplay(display, (uint32_t)options);
}

CGError CGDisplayRelease(CGDirectDisplayID display) {
    return display == 0 ? kCGErrorIllegalArgument : slsRelease(display);
}

CGError CGCaptureAllDisplays(void) {
    return CGCaptureAllDisplaysWithOptions(kCGCaptureNoOptions);
}

CGError CGCaptureAllDisplaysWithOptions(CGCaptureOptions options) {
    return (CGError)SLSCaptureDisplay(0, (uint32_t)options);
}

CGError CGReleaseAllDisplays(void) {
    return slsRelease(0);
}

CGWindowID CGShieldingWindowID(CGDirectDisplayID display) {
    // Not implemented
    return 0;
}

CGWindowLevel CGShieldingWindowLevel(void) {
    // Not implemented
    return 0;
}

CGContextRef CGDisplayGetDrawingContext(CGDirectDisplayID display) {
    return slsDrawingContext(display);
}

// Creating Images from the Display
CGImageRef CGDisplayCreateImage(CGDirectDisplayID display) {
    CGRect rect = CGDisplayBounds(display);
    return CGDisplayCreateImageForRect(display, rect);
}

CGImageRef CGDisplayCreateImageForRect(CGDirectDisplayID display, CGRect rect) {
    return slsDisplayImage(display, rect);
}

// Configuring Displays
CGError CGBeginDisplayConfiguration(CGDisplayConfigRef *config) {
    if(config == NULL)
        return kCGErrorIllegalArgument;
    *config = calloc(sizeof(struct _CGDisplayConfig), 1);
    if(*config == NULL)
        return kCGErrorFailure;
    CGError err = SLSBeginDisplayConfiguration(&(*config)->sls);
    if(err != kCGErrorSuccess) {
        free(*config);
        *config = NULL;
    }
    return err;
}

CGError CGCancelDisplayConfiguration(CGDisplayConfigRef config) {
    if(config == NULL)
        return kCGErrorIllegalArgument;
    CGError err = SLSCancelDisplayConfiguration(config->sls);
    free(config);
    return err;
}

CGError CGCompleteDisplayConfiguration(CGDisplayConfigRef config, CGConfigureOption option) {
    CGDisplayFadeReservationToken fade = kCGDisplayFadeReservationInvalidToken;

    if(config == NULL)
        return kCGErrorIllegalArgument;
    if((config->fadeOut > 0 || config->fadeIn > 0) &&
        CGAcquireDisplayFadeReservation(config->fadeOut + config->fadeIn + 2, &fade) == kCGErrorSuccess)
        CGDisplayFade(fade, config->fadeOut, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor,
            config->fadeColor[0], config->fadeColor[1], config->fadeColor[2], true);
    CGError err = SLSCompleteDisplayConfiguration(config->sls, (uint32_t)option);
    if(fade != kCGDisplayFadeReservationInvalidToken) {
        CGDisplayFade(fade, config->fadeIn, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal,
            config->fadeColor[0], config->fadeColor[1], config->fadeColor[2], true);
        CGReleaseDisplayFadeReservation(fade);
    }
    free(config);
    return err;
}

CGError CGConfigureDisplayMirrorOfDisplay(CGDisplayConfigRef config, CGDirectDisplayID display, CGDirectDisplayID primary) {
    if(config == NULL || display == kCGNullDirectDisplay)
        return kCGErrorIllegalArgument;
    return SLSConfigureDisplayMirrorOfDisplay(config->sls, display, primary);
}

CGError CGConfigureDisplayOrigin(CGDisplayConfigRef config, CGDirectDisplayID display, int32_t x, int32_t y) {
    if(config == NULL || display == kCGNullDirectDisplay)
        return kCGErrorIllegalArgument;
    return SLSConfigureDisplayOrigin(config->sls, display, x, y);
}

/* SHORTCUT(WS5) (the window server's): configurations last for the
   session, so there is no permanent one to go back to. */
void CGRestorePermanentDisplayConfiguration(void) {
}

CGError CGConfigureDisplayStereoOperation(CGDisplayConfigRef config, CGDirectDisplayID display, boolean_t stereo, boolean_t forceBlueLine) {
    return kCGErrorFailure; // not implemented
}

CGError CGDisplaySetStereoOperation(CGDirectDisplayID display, boolean_t stereo, boolean_t forceBlueLine, CGConfigureOption option) {
    return kCGErrorFailure; // not implemented
}

CGError CGConfigureDisplayWithDisplayMode(CGDisplayConfigRef config, CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options) {
    if(config == NULL || display == kCGNullDirectDisplay || mode == NULL)
        return kCGErrorIllegalArgument;
    return SLSConfigureDisplayWithDisplayMode(config->sls, display, mode->modeID);
}

// Getting the Display Configuration
CGColorSpaceRef CGDisplayCopyColorSpace(CGDirectDisplayID display) {
    // FIXME: implement this properly, then use it in drawing ctx/image for rect
    return CGColorSpaceRetain(CGColorSpaceCreateDeviceRGB());
}

boolean_t CGDisplayIsActive(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayActive);
}

boolean_t CGDisplayIsAlwaysInMirrorSet(CGDirectDisplayID display) {
    return FALSE;
}

boolean_t CGDisplayIsAsleep(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplaySleeping);
}

boolean_t CGDisplayIsBuiltin(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayBuiltin);
}

boolean_t CGDisplayIsInHWMirrorSet(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayHWMirror);
}

boolean_t CGDisplayIsInMirrorSet(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayMirrored);
}

boolean_t CGDisplayIsMain(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayMain);
}

boolean_t CGDisplayIsOnline(CGDirectDisplayID display) {
    return (CGDisplayStateFlags(display) & kSLSDisplayOnline);
}

boolean_t CGDisplayIsStereo(CGDirectDisplayID display) {
    return FALSE; // not implemented
}

CGDirectDisplayID CGDisplayMirrorsDisplay(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return kCGNullDirectDisplay;
    return info.mirrorOf;
}

uint32_t CGDisplayModelNumber(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0xFFFFFFFF;
    return info.model;
}

uint32_t CGDisplayPrimaryDisplay(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return display;
    return info.primary ? info.primary : display;
}

double CGDisplayRotation(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0;
    return info.rotation;
}

CGSize CGDisplayScreenSize(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return CGSizeZero;
    return CGSizeMake(info.widthMM, info.heightMM);
}

uint32_t CGDisplaySerialNumber(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0xFFFFFFFF;
    return info.serial;
}

uint32_t CGDisplayUnitNumber(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0xFFFFFFFF;
    return info.unit;
}

boolean_t CGDisplayUsesOpenGLAcceleration(CGDirectDisplayID display) {
    return FALSE; // FIXME: implement with Quartz Extreme
}

uint32_t CGDisplayVendorNumber(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0xFFFFFFFF;
    return info.vendor;
}

// Registering for Notification of Config Changes
CGError CGDisplayRegisterReconfigurationCallback(CGDisplayReconfigurationCallBack callback, void *userInfo) {
    return kCGErrorFailure; // FIXME: not yet implemented
}

CGError CGDisplayRemoveReconfigurationCallback(CGDisplayReconfigurationCallBack callback, void *userInfo) {
    return kCGErrorFailure; // FIXME: not yet implemented
}

// Retrieving Display Parameters
// FIXME: these are returning info on the current mode, not the display itself
CGRect CGDisplayBounds(CGDirectDisplayID display) {
    SLSDisplayInfo info;
    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return NSZeroRect;
    return NSMakeRect(info.x, info.y, info.width, info.height);
}

size_t CGDisplayPixelsHigh(CGDirectDisplayID display) {
    CGRect rect = CGDisplayBounds(display);
    return rect.size.height;
}

size_t CGDisplayPixelsWide(CGDirectDisplayID display) {
    CGRect rect = CGDisplayBounds(display);
    return rect.size.width;
}

// Creating and Managing Display Modes
CGDisplayModeRef CGDisplayCopyDisplayMode(CGDirectDisplayID display) {
    SLSDisplayMode m;

    if(SLSGetCurrentDisplayMode(display, &m) != kSLSErrorSuccess)
        return NULL;
    return displayModeCreate(&m);
}

CFArrayRef CGDisplayCopyAllDisplayModes(CGDirectDisplayID display, CFDictionaryRef options) {
    SLSDisplayMode modes[kSLSMaxDisplayModes];
    uint32_t count = 0;

    if(SLSCopyDisplayModes(display, kSLSMaxDisplayModes, modes, &count) != kSLSErrorSuccess)
        return NULL;
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, count, &displayModeCallBacks);
    for(uint32_t i = 0; i < count; i++) {
        CGDisplayModeRef mode = displayModeCreate(&modes[i]);
        if(mode != NULL) {
            CFArrayAppendValue(result, mode);
            CGDisplayModeRelease(mode);
        }
    }
    return result;
}

/* Until the app quits or the display is configured again, as the
   reference platform does. */
CGError CGDisplaySetDisplayMode(CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options) {
    CGDisplayConfigRef config;
    CGError err;

    if(mode == NULL)
        return kCGErrorIllegalArgument;
    if((err = CGBeginDisplayConfiguration(&config)) != kCGErrorSuccess)
        return err;
    if((err = CGConfigureDisplayWithDisplayMode(config, display, mode, options)) != kCGErrorSuccess) {
        CGCancelDisplayConfiguration(config);
        return err;
    }
    return CGCompleteDisplayConfiguration(config, kCGConfigureForAppOnly);
}

CGDisplayModeRef CGDisplayModeRetain(CGDisplayModeRef mode) {
    if(mode != NULL) {
        int one = 1;
        __sync_fetch_and_add(&(mode->refcount), one);
    }
    return mode;
}

void CGDisplayModeRelease(CGDisplayModeRef mode) {
    if(mode == NULL)
        return;

    int one = 1;
    int count = __sync_fetch_and_sub(&(mode->refcount), one);

    if(count == 1) // we just released the last ref
        free(mode);
}

// Getting Information About a Display Mode
size_t CGDisplayModeGetWidth(CGDisplayModeRef mode) {
    if(mode == NULL)
        return 0;
    return mode->width;
}

size_t CGDisplayModeGetHeight(CGDisplayModeRef mode) {
    if(mode == NULL)
        return 0;
    return mode->height;
}

double CGDisplayModeGetRefreshRate(CGDisplayModeRef mode) {
    if(mode == NULL)
        return 0;
    return mode->refresh;
}

uint32_t CGDisplayModeGetIOFlags(CGDisplayModeRef mode) {
    if(mode == NULL)
        return 0;
    return mode->flags;
}

int32_t CGDisplayModeGetIODisplayModeID(CGDisplayModeRef mode) {
    if(mode == NULL)
        return 0;
    return (int32_t)mode->modeID;
}

bool CGDisplayModeIsUsableForDesktopGUI(CGDisplayModeRef mode) {
    if(mode == NULL)
        return false;
    if(mode->width >= 1024 && mode->height >= 768 && mode->depth >= 24)
        return true;
    return false;
}

CFTypeID CGDisplayModeGetTypeID(void) {
    return 0; // FIXME
}

// Adjusting the Display Gamma
CGError CGSetDisplayTransferByFormula(CGDirectDisplayID display,
        CGGammaValue redMin, CGGammaValue redMax, CGGammaValue redGamma,
        CGGammaValue greenMin, CGGammaValue greenMax, CGGammaValue greenGamma,
        CGGammaValue blueMin, CGGammaValue blueMax, CGGammaValue blueGamma) {
    if(redMin > redMax || redMin < 0 ||
        greenMin > greenMax || greenMin < 0 ||
        blueMin > blueMax || blueMin < 0 ||
        redMax > 1 || greenMax > 1 || blueMax > 1)
            return kCGErrorIllegalArgument;

    SLSGammaFormula f = { redMin, redMax, redGamma, greenMin, greenMax, greenGamma,
        blueMin, blueMax, blueGamma };
    return SLSSetDisplayTransferByFormula(display, &f);
}

CGError CGGetDisplayTransferByFormula(CGDirectDisplayID display,
        CGGammaValue *redMin, CGGammaValue *redMax, CGGammaValue *redGamma,
        CGGammaValue *greenMin, CGGammaValue *greenMax, CGGammaValue *greenGamma,
        CGGammaValue *blueMin, CGGammaValue *blueMax, CGGammaValue *blueGamma) {
    if(redMin == NULL || redMax == NULL || redGamma == NULL ||
        greenMin == NULL || greenMax == NULL || greenGamma == NULL ||
        blueMin == NULL || blueMax == NULL || blueGamma == NULL)
            return kCGErrorIllegalArgument;

    SLSGammaFormula f;
    CGError err = SLSGetDisplayTransferByFormula(display, &f);
    if(err != kCGErrorSuccess)
        return err;
    *redMin = f.redMin; *redMax = f.redMax; *redGamma = f.redGamma;
    *greenMin = f.greenMin; *greenMax = f.greenMax; *greenGamma = f.greenGamma;
    *blueMin = f.blueMin; *blueMax = f.blueMax; *blueGamma = f.blueGamma;
    return kCGErrorSuccess;
}

CGError CGSetDisplayTransferByTable(CGDirectDisplayID display, uint32_t tableSize,
        const CGGammaValue *redTable, const CGGammaValue *greenTable, const CGGammaValue *blueTable) {
    if(redTable == NULL || greenTable == NULL || blueTable == NULL)
        return kCGErrorIllegalArgument;
    if(tableSize == 0)
        return kCGErrorSuccess; // nothing to do
    return SLSSetDisplayTransferByTable(display, tableSize, redTable, greenTable, blueTable);
}


CGError CGGetDisplayTransferByTable(CGDirectDisplayID display, uint32_t capacity,
        CGGammaValue *redTable, CGGammaValue *greenTable, CGGammaValue *blueTable, uint32_t *sampleCount) {
    if(redTable == NULL || greenTable == NULL || blueTable == NULL || capacity == 0 ||
        sampleCount == NULL)
        return kCGErrorIllegalArgument;
    return SLSGetDisplayTransferByTable(display, capacity, redTable, greenTable, blueTable,
        sampleCount);
}

CGError CGSetDisplayTransferByByteTable(CGDirectDisplayID display, uint32_t tableSize,
        const uint8_t *redTable, const uint8_t *greenTable, const uint8_t *blueTable) {
    if(redTable == NULL || greenTable == NULL || blueTable == NULL)
        return kCGErrorIllegalArgument;
    if(tableSize == 0)
        return kCGErrorSuccess; // nothing to do

    CGGammaValue *values = calloc(3 * tableSize, sizeof(CGGammaValue));
    if(values == NULL)
        return kCGErrorFailure;
    for(uint32_t i = 0; i < tableSize; i++) {
        values[i] = redTable[i] / 255.0f;
        values[tableSize + i] = greenTable[i] / 255.0f;
        values[2 * tableSize + i] = blueTable[i] / 255.0f;
    }
    CGError err = SLSSetDisplayTransferByTable(display, tableSize, values, values + tableSize,
        values + 2 * tableSize);
    free(values);
    return err;
}

void CGDisplayRestoreColorSyncSettings(void) {
    SLSRestoreDisplayTransfers();
}

uint32_t CGDisplayGammaTableCapacity(CGDirectDisplayID display) {
    uint32_t capacity = 0;

    if(SLSGetGammaTableCapacity(display, &capacity) != kSLSErrorSuccess)
        return 0;
    return capacity;
}

// Display Fade Effects -- all stubs for now!
/* The configuration fades the displays out, changes, then fades them in
   (CGCompleteDisplayConfiguration). */
CGError CGConfigureDisplayFadeEffect(CGDisplayConfigRef config, CGDisplayFadeInterval fadeOutSeconds, CGDisplayFadeInterval fadeInSeconds, float fadeRed, float fadeGreen, float fadeBlue) {
    if(config == NULL || fadeOutSeconds < 0 || fadeInSeconds < 0)
        return kCGErrorIllegalArgument;
    config->fadeOut = fadeOutSeconds;
    config->fadeIn = fadeInSeconds;
    config->fadeColor[0] = fadeRed;
    config->fadeColor[1] = fadeGreen;
    config->fadeColor[2] = fadeBlue;
    return kCGErrorSuccess;
}

CGError CGAcquireDisplayFadeReservation(CGDisplayReservationInterval seconds, CGDisplayFadeReservationToken *token) {
    SLSFadeReservation reservation;

    if(token == NULL)
        return kCGErrorIllegalArgument;
    CGError err = SLSAcquireFadeReservation(seconds, &reservation);
    *token = (err == kCGErrorSuccess) ? (CGDisplayFadeReservationToken)reservation
                                      : kCGDisplayFadeReservationInvalidToken;
    return err;
}

CGError CGDisplayFade(CGDisplayFadeReservationToken token, CGDisplayFadeInterval duration, CGDisplayBlendFraction startBlend, CGDisplayBlendFraction endBlend, float redBlend, float greenBlend, float blueBlend, boolean_t synchronous) {
    return SLSFadeDisplays((SLSFadeReservation)token, duration, startBlend, endBlend,
        redBlend, greenBlend, blueBlend, synchronous ? 1 : 0);
}

CGError CGReleaseDisplayFadeReservation(CGDisplayFadeReservationToken token) {
    return SLSReleaseFadeReservation((SLSFadeReservation)token);
}

// Controlling the Mouse Cursor
CGError CGDisplayHideCursor(CGDirectDisplayID display) {
    return SLSHideCursor(display);
}

CGError CGDisplayShowCursor(CGDirectDisplayID display) {
    return SLSShowCursor(display);
}

/* point is from the display's top left. */
CGError CGDisplayMoveCursorToPoint(CGDirectDisplayID display, CGPoint point) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return kCGErrorIllegalArgument;
    return SLSWarpCursorPosition((SLSPoint){ info.x + point.x, info.y + point.y });
}

CGError CGAssociateMouseAndMouseCursorPosition(boolean_t connected) {
    return SLSAssociateMouseAndCursorPosition(connected ? 1 : 0);
}

CGError CGWarpMouseCursorPosition(CGPoint newCursorPosition) {
    return SLSWarpCursorPosition((SLSPoint){ newCursorPosition.x, newCursorPosition.y });
}

void CGGetLastMouseDelta(int32_t *deltaX, int32_t *deltaY) {
    int32_t dx = 0, dy = 0;

    SLSGetLastMouseDelta(&dx, &dy);
    if(deltaX != NULL)
        *deltaX = dx;
    if(deltaY != NULL)
        *deltaY = dy;
}


// WindowServer info
CFDictionaryRef CGSessionCopyCurrentDictionary(void) {

}

// This function resolves the bootstrap service and returns a port with send rights
// for calling WindowServer. The port must not be deallocated or modified!
CFMachPortRef CGWindowServerCFMachPort(void) {
    mach_port_t port = SLSServerPort();

    if(port == MACH_PORT_NULL)
        return NULL;
    return CFMachPortCreateWithPort(NULL, port, NULL, NULL, NULL);
}

// The values below were obtained by testing macOS Sonoma
CGWindowLevel CGWindowLevelForKey(CGWindowLevelKey key) {
    if(key < 0 || key >= kCGNumberOfWindowLevelKeys)
        return -INT_MAX;
    const int32_t levels[kCGNumberOfWindowLevelKeys] = {
        -INT_MAX, -INT_MAX, -INT_MAX, -20, 0,
        3, 3, 20, 24, 25, 8, 101, 500, 1000,
        INT_MAX, 102, 200, 19, -INT_MAX, INT_MAX, 1500
    };
    return levels[key];
}

// Streaming the Display Contents -- all stubs for now! (deprecated after 14.4)

CGDisplayStreamRef CGDisplayStreamCreate(CGDirectDisplayID display, size_t outputWidth, size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties, CGDisplayStreamFrameAvailableHandler handler) {
    return NULL;
}

CGDisplayStreamRef CGDisplayStreamCreateWithDispatchQueue(CGDirectDisplayID display, size_t outputWidth, size_t outputHeight, int32_t pixelFormat, CFDictionaryRef properties, dispatch_queue_t queue, CGDisplayStreamFrameAvailableHandler handler) {
    return NULL;
}

CGError CGDisplayStreamStart(CGDisplayStreamRef displayStream) {
    return kCGErrorFailure;
}

CGError CGDisplayStreamStop(CGDisplayStreamRef displayStream) {
    return kCGErrorSuccess;
}

CFRunLoopSourceRef CGDisplayStreamGetRunLoopSource(CGDisplayStreamRef displayStream) {
    return NULL;
}

const CGRect * CGDisplayStreamUpdateGetRects(CGDisplayStreamUpdateRef updateRef, CGDisplayStreamUpdateRectType rectType, size_t *rectCount) {
    return NULL;
}

CGDisplayStreamUpdateRef CGDisplayStreamUpdateCreateMergedUpdate(CGDisplayStreamUpdateRef firstUpdate, CGDisplayStreamUpdateRef secondUpdate) {
    return NULL;
}

void CGDisplayStreamUpdateGetMovedRectsDelta(CGDisplayStreamUpdateRef updateRef, CGFloat *dx, CGFloat *dy) {
}

size_t CGDisplayStreamUpdateGetDropCount(CGDisplayStreamUpdateRef updateRef) {
    return 0;
}

CFTypeID CGDisplayStreamGetTypeID(void) {
    return 0;
}

CFTypeID CGDisplayStreamUpdateGetTypeID(void) {
    return 0;
}

// Private functions

static uint32_t CGDisplayStateFlags(CGDirectDisplayID display) {
    SLSDisplayInfo info;

    if(SLSGetDisplayInfo(display, &info) != kSLSErrorSuccess)
        return 0;
    return info.flags;
}
