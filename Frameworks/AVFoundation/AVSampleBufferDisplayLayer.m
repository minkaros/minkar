/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The sample buffer display layer. Each frame enqueued becomes the
 * layer's contents: its IOSurface (Core Animation draws YCbCr surfaces
 * too), or an image made from a buffer in plain memory. A frame marked to
 * be shown at once, or any frame while the layer has no control timebase,
 * is shown as it comes; the others wait, in presentation order, for their
 * time on the timebase, through a timer of the timebase (which follows its
 * rate and jumps). Compressed samples are decoded first, with a
 * decompression session made for their format. The frame shown and the
 * one before stay held, so a decoder's pool does not reuse their memory
 * while the window server may still draw them.
 */

#import <AVFoundation/AVSampleBufferDisplayLayer.h>
#import <AVFoundation/AVError.h>
#import <CoreVideo/CoreVideo.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOSurface/IOSurface.h>
#import <VideoToolbox/VideoToolbox.h>
#include <pthread.h>

/* QuartzCore (CAYUVImage.c): a YCbCr buffer in plain memory as an image. */
extern CGImageRef _CACreateImageFromYCbCrPlanes(OSType format, size_t width, size_t height,
                                                const void *const planes[3], const size_t strides[3], int bt709);

AVLayerVideoGravity const AVLayerVideoGravityResizeAspect = @"AVLayerVideoGravityResizeAspect";
AVLayerVideoGravity const AVLayerVideoGravityResizeAspectFill = @"AVLayerVideoGravityResizeAspectFill";
AVLayerVideoGravity const AVLayerVideoGravityResize = @"AVLayerVideoGravityResize";
NSNotificationName const AVSampleBufferDisplayLayerFailedToDecodeNotification =
    @"AVSampleBufferDisplayLayerFailedToDecodeNotification";
NSString *const AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey =
    @"AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey";

/* Frames waiting for their time, at most: below it the layer wants more. */
enum { kQueueLimit = 30 };
/* The display stays awake this long after the last frame shown. */
static const int64_t kAwakeAfterLastFrame = 2 * NSEC_PER_SEC;

static NSString *gravityFor(AVLayerVideoGravity g) {
    if ([g isEqual:AVLayerVideoGravityResizeAspectFill])
        return kCAGravityResizeAspectFill;
    if ([g isEqual:AVLayerVideoGravityResize])
        return kCAGravityResize;
    return kCAGravityResizeAspect;
}

static BOOL attachmentIsTrue(CMSampleBufferRef sample, CFStringRef key) {
    CFArrayRef all = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (all == NULL || CFArrayGetCount(all) == 0)
        return NO;
    CFDictionaryRef first = CFArrayGetValueAtIndex(all, 0);
    CFTypeRef v = first != NULL ? CFDictionaryGetValue(first, key) : NULL;
    return v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v);
}

/* A frame waiting: its image and when to show it. */
@interface AVDisplayLayerFrame : NSObject {
@public
    CVImageBufferRef image;
    CMTime time;
}
@end

@implementation AVDisplayLayerFrame
- (void)dealloc {
    if (image != NULL)
        CVBufferRelease(image);
}
@end

/* What the timebase's notifications reach: the layer, weakly. */
@interface AVDisplayLayerListener : NSObject {
@public
    __weak AVSampleBufferDisplayLayer *layer;
}
@end

@implementation AVDisplayLayerListener
@end

@interface AVSampleBufferDisplayLayer ()
- (void)_timebaseChanged;
- (void)_decoded:(CVImageBufferRef)image time:(CMTime)time immediately:(BOOL)now status:(OSStatus)status;
@end

typedef struct {
    void *layer;        /* the layer, not retained: the session goes before it */
    BOOL immediately;
} DecodeContext;

static void decodedFrame(void *refCon, void *frameRefCon, OSStatus status, VTDecodeInfoFlags flags,
                         CVImageBufferRef image, CMTime time, CMTime duration) {
    (void)frameRefCon;
    (void)duration;
    DecodeContext *c = refCon;
    if ((flags & kVTDecodeInfo_FrameDropped) != 0 && status == noErr)
        return;
    [(__bridge AVSampleBufferDisplayLayer *)c->layer _decoded:image time:time immediately:c->immediately
                                                       status:status];
}

@implementation AVSampleBufferDisplayLayer {
    /* The state below, held by any thread that touches it; recursive, so
     * an observer of status or a decoder's answer may call the layer back. */
    pthread_mutex_t _lock;
    dispatch_queue_t _queue;            /* the timers' */
    NSMutableArray<AVDisplayLayerFrame *> *_waiting;
    dispatch_source_t _timer;           /* on _timerBase */
    CMTimebaseRef _timerBase;
    CMTimebaseRef _controlTimebase;     /* retained */
    AVLayerVideoGravity _videoGravity;
    CVImageBufferRef _held[2];
    dispatch_queue_t _requestQueue;
    void (^_requestBlock)(void);
    BOOL _wasReady;
    VTDecompressionSessionRef _decoder;
    CMFormatDescriptionRef _decoderFormat;
    DecodeContext _context;
    IOPMAssertionID _awake;
    uint64_t _lastShown;
    AVDisplayLayerListener *_listener;
}

@synthesize status = _status;
@synthesize error = _error;

- (instancetype)init {
    if ((self = [super init]) != nil) {
        pthread_mutexattr_t a;
        pthread_mutexattr_init(&a);
        pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&_lock, &a);
        pthread_mutexattr_destroy(&a);
        _queue = dispatch_queue_create("org.minkar.AVSampleBufferDisplayLayer", DISPATCH_QUEUE_SERIAL);
        _waiting = [NSMutableArray array];
        _videoGravity = AVLayerVideoGravityResizeAspect;
        _preventsDisplaySleepDuringVideoPlayback = YES;
        _wasReady = YES;
        _awake = kIOPMNullAssertionID;
        _listener = [AVDisplayLayerListener new];
        _listener->layer = self;
        self.contentsGravity = kCAGravityResizeAspect;
    }
    return self;
}

- (void)dealloc {
    [self _dropDecoder];
    [self _setTimerBase:NULL];
    for (int i = 0; i < 2; i++)
        if (_held[i] != NULL)
            CVBufferRelease(_held[i]);
    if (_controlTimebase != NULL)
        CFRelease(_controlTimebase);
    if (_awake != kIOPMNullAssertionID)
        IOPMAssertionRelease(_awake);
    pthread_mutex_destroy(&_lock);
}

- (void)_locked:(void (NS_NOESCAPE ^)(void))block {
    pthread_mutex_lock(&_lock);
    block();
    pthread_mutex_unlock(&_lock);
}

/* ---- Properties ---- */

- (AVLayerVideoGravity)videoGravity {
    __block AVLayerVideoGravity g;
    [self _locked:^{ g = self->_videoGravity; }];
    return g;
}

- (void)setVideoGravity:(AVLayerVideoGravity)gravity {
    AVLayerVideoGravity g = [gravity copy] ?: AVLayerVideoGravityResizeAspect;
    [self _locked:^{ self->_videoGravity = g; }];
    self.contentsGravity = gravityFor(g);
}

- (CMTimebaseRef)controlTimebase {
    __block CMTimebaseRef t;
    [self _locked:^{ t = self->_controlTimebase; }];
    return t;
}

- (void)setControlTimebase:(CMTimebaseRef)timebase {
    [self _locked:^{
        if (timebase == self->_controlTimebase)
            return;
        if (timebase != NULL)
            CFRetain(timebase);
        if (self->_controlTimebase != NULL)
            CFRelease(self->_controlTimebase);
        self->_controlTimebase = timebase;
        [self _setTimerBase:timebase];
        [self _showDue];
    }];
}

- (CMTimebaseRef)timebase {
    return self.controlTimebase;
}

- (AVQueuedSampleBufferRenderingStatus)status {
    __block AVQueuedSampleBufferRenderingStatus s;
    [self _locked:^{ s = self->_status; }];
    return s;
}

- (NSError *)error {
    __block NSError *e;
    [self _locked:^{ e = self->_error; }];
    return e;
}

- (BOOL)isReadyForMoreMediaData {
    __block BOOL ready;
    [self _locked:^{ ready = self->_waiting.count < kQueueLimit; }];
    return ready;
}

- (BOOL)hasSufficientMediaDataForReliablePlaybackStart {
    __block BOOL enough;
    [self _locked:^{ enough = self->_waiting.count > 0 || self->_held[0] != NULL; }];
    return enough;
}

/* ---- Taking samples ---- */

- (void)enqueueSampleBuffer:(CMSampleBufferRef)sample {
    if (sample == NULL)
        return;
    CFRetain(sample);
    [self _locked:^{
        [self _enqueue:sample];
    }];
    CFRelease(sample);
}

/* Locked. */
- (void)_enqueue:(CMSampleBufferRef)sample {
    if (_status == AVQueuedSampleBufferRenderingStatusFailed)
        return;
    BOOL now = attachmentIsTrue(sample, kCMSampleAttachmentKey_DisplayImmediately);
    BOOL hidden = attachmentIsTrue(sample, kCMSampleAttachmentKey_DoNotDisplay);
    CVImageBufferRef image = CMSampleBufferGetImageBuffer(sample);

    [self _setStatus:AVQueuedSampleBufferRenderingStatusRendering error:nil];
    if (image != NULL) {
        if (!hidden)
            [self _take:image time:CMSampleBufferGetPresentationTimeStamp(sample) immediately:now];
        return;
    }
    /* Compressed: decoded here; a hidden one still feeds the decoder. */
    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);
    if (format == NULL || CMFormatDescriptionGetMediaType(format) != kCMMediaType_Video) {
        [self _failWith:kVTVideoDecoderBadDataErr];
        return;
    }
    if (_decoder != NULL && !CMFormatDescriptionEqual(format, _decoderFormat) &&
        !VTDecompressionSessionCanAcceptFormatDescription(_decoder, format))
        [self _dropDecoder];
    if (_decoder == NULL) {
        _context.layer = (__bridge void *)self;
        VTDecompressionOutputCallbackRecord callback = { decodedFrame, &_context };
        OSStatus err = VTDecompressionSessionCreate(NULL, format, NULL, NULL, &callback, &_decoder);
        if (err != noErr) {
            _decoder = NULL;
            [self _failWith:err];
            return;
        }
        _decoderFormat = (CMFormatDescriptionRef)CFRetain(format);
    }
    /* The callback comes within the call: the decoder answers each frame
     * as it is given. */
    _context.immediately = now;
    VTDecodeInfoFlags info = 0;
    OSStatus err = VTDecompressionSessionDecodeFrame(_decoder, sample,
        hidden ? kVTDecodeFrame_DoNotOutputFrame : 0, NULL, &info);
    if (err != noErr)
        [self _failWith:err];
}

/* From the decoder, locked. */
- (void)_decoded:(CVImageBufferRef)image time:(CMTime)time immediately:(BOOL)now status:(OSStatus)status {
    if (status != noErr) {
        [self _failWith:status];
        return;
    }
    if (image != NULL)
        [self _take:image time:time immediately:now];
}

/* Locked: shown now, or put in its place among those waiting. */
- (void)_take:(CVImageBufferRef)image time:(CMTime)time immediately:(BOOL)now {
    if (now || _controlTimebase == NULL || !CMTIME_IS_NUMERIC(time)) {
        [self _show:image];
        return;
    }
    AVDisplayLayerFrame *f = [AVDisplayLayerFrame new];
    f->image = CVBufferRetain(image);
    f->time = time;
    NSUInteger i = _waiting.count;
    while (i > 0 && CMTimeCompare(_waiting[i - 1]->time, time) > 0)
        i--;
    [_waiting insertObject:f atIndex:i];
    [self _showDue];
}

/* Locked: the latest frame whose time has come is shown, the earlier
 * ones dropped; the timer is set for the next. */
- (void)_showDue {
    if (_controlTimebase == NULL) {
        AVDisplayLayerFrame *last = _waiting.lastObject;
        if (last != nil)
            [self _show:last->image];
        [_waiting removeAllObjects];
        [self _noteReadiness];
        return;
    }
    CMTime now = CMTimebaseGetTime(_controlTimebase);
    NSUInteger due = 0;
    while (due < _waiting.count && CMTimeCompare(_waiting[due]->time, now) <= 0)
        due++;
    if (due > 0) {
        [self _show:_waiting[due - 1]->image];
        [_waiting removeObjectsInRange:NSMakeRange(0, due)];
    }
    if (_timer != NULL) {
        if (_waiting.count > 0)
            CMTimebaseSetTimerDispatchSourceNextFireTime(_timerBase, _timer, _waiting[0]->time, 0);
        else
            dispatch_source_set_timer(_timer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
    }
    [self _noteReadiness];
}

/* The timebase jumped or changed rate: what is due is shown (a frame
 * sought while paused too), and the timer set again. */
static void timebaseChanged(CMNotificationCenterRef center, const void *listener, CFStringRef name,
                            const void *object, CFTypeRef payload) {
    (void)center;
    (void)name;
    (void)object;
    (void)payload;
    [((__bridge AVDisplayLayerListener *)listener)->layer _timebaseChanged];
}

- (void)_timebaseChanged {
    __weak AVSampleBufferDisplayLayer *weakSelf = self;
    dispatch_async(_queue, ^{
        AVSampleBufferDisplayLayer *me = weakSelf;
        [me _locked:^{
            if (me->_timerBase != NULL)
                [me _showDue];
        }];
    });
}

/* Locked: the timer follows the timebase. */
- (void)_setTimerBase:(CMTimebaseRef)timebase {
    CMNotificationCenterRef center = CMNotificationCenterGetDefaultLocalCenter();
    if (_timer != NULL) {
        CMTimebaseRemoveTimerDispatchSource(_timerBase, _timer);
        dispatch_source_cancel(_timer);
        _timer = NULL;
    }
    if (_timerBase != NULL) {
        CMNotificationCenterRemoveListener(center, (__bridge const void *)_listener, timebaseChanged,
                                           kCMTimebaseNotification_TimeJumped, _timerBase);
        CMNotificationCenterRemoveListener(center, (__bridge const void *)_listener, timebaseChanged,
                                           kCMTimebaseNotification_EffectiveRateChanged, _timerBase);
        CFRelease(_timerBase);
    }
    if (timebase != NULL)
        CFRetain(timebase);
    _timerBase = timebase;
    if (timebase == NULL)
        return;
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    __weak AVSampleBufferDisplayLayer *weakSelf = self;
    dispatch_source_set_event_handler(_timer, ^{
        AVSampleBufferDisplayLayer *me = weakSelf;
        [me _locked:^{
            [me _showDue];
        }];
    });
    CMTimebaseAddTimerDispatchSource(timebase, _timer);
    dispatch_resume(_timer);
    CMNotificationCenterAddListener(center, (__bridge const void *)_listener, timebaseChanged,
                                    kCMTimebaseNotification_TimeJumped, timebase, 0);
    CMNotificationCenterAddListener(center, (__bridge const void *)_listener, timebaseChanged,
                                    kCMTimebaseNotification_EffectiveRateChanged, timebase, 0);
}

/* Locked: the frame becomes the contents. */
- (void)_show:(CVImageBufferRef)image {
    id contents = nil;
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(image);
    if (surface != NULL)
        contents = (__bridge id)surface;
    else {
        CGImageRef made = [self _imageOf:image];
        if (made == NULL)
            return;
        contents = (__bridge_transfer id)made;
    }
    if (_held[1] != NULL)
        CVBufferRelease(_held[1]);
    _held[1] = _held[0];
    _held[0] = CVBufferRetain(image);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.contents = contents;
    [CATransaction commit];
    [self _stayAwake];
}

/* A buffer in plain memory as an image: BGRA as it is, YCbCr converted. */
- (CGImageRef)_imageOf:(CVImageBufferRef)image {
    CVPixelBufferRef b = (CVPixelBufferRef)image;
    OSType format = CVPixelBufferGetPixelFormatType(b);
    size_t w = CVPixelBufferGetWidth(b), h = CVPixelBufferGetHeight(b);
    CGImageRef made = NULL;

    if (CVPixelBufferLockBaseAddress(b, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
        return NULL;
    if (format == kCVPixelFormatType_32BGRA || format == kCVPixelFormatType_32ARGB) {
        size_t rowBytes = CVPixelBufferGetBytesPerRow(b);
        CFDataRef pixels = CFDataCreate(NULL, CVPixelBufferGetBaseAddress(b), (CFIndex)(rowBytes * h));
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(pixels);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGBitmapInfo info = format == kCVPixelFormatType_32BGRA
            ? kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
            : kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big;
        made = CGImageCreate(w, h, 8, 32, rowBytes, space, info, provider, NULL, false, kCGRenderingIntentDefault);
        CGColorSpaceRelease(space);
        CGDataProviderRelease(provider);
        CFRelease(pixels);
    } else {
        const void *planes[3] = { NULL, NULL, NULL };
        size_t strides[3] = { 0, 0, 0 };
        size_t count = CVPixelBufferGetPlaneCount(b);
        if (count == 0) {
            planes[0] = CVPixelBufferGetBaseAddress(b);
            strides[0] = CVPixelBufferGetBytesPerRow(b);
        }
        for (size_t i = 0; i < count && i < 3; i++) {
            planes[i] = CVPixelBufferGetBaseAddressOfPlane(b, i);
            strides[i] = CVPixelBufferGetBytesPerRowOfPlane(b, i);
        }
        int bt709 = -1;
        CFTypeRef matrix = CVBufferGetAttachment(b, kCVImageBufferYCbCrMatrixKey, NULL);
        if (matrix != NULL)
            bt709 = CFEqual(matrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2) ? 1 : 0;
        made = _CACreateImageFromYCbCrPlanes(format, w, h, planes, strides, bt709);
    }
    CVPixelBufferUnlockBaseAddress(b, kCVPixelBufferLock_ReadOnly);
    return made;
}

/* Locked: while frames come, the display does not sleep. */
- (void)_stayAwake {
    _lastShown = dispatch_time(DISPATCH_TIME_NOW, 0);
    if (!_preventsDisplaySleepDuringVideoPlayback || _awake != kIOPMNullAssertionID)
        return;
    if (IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                    CFSTR("Video playback"), &_awake) != kIOReturnSuccess) {
        _awake = kIOPMNullAssertionID;
        return;
    }
    [self _checkAwake];
}

- (void)_checkAwake {
    __weak AVSampleBufferDisplayLayer *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kAwakeAfterLastFrame), _queue, ^{
        AVSampleBufferDisplayLayer *me = weakSelf;
        [me _locked:^{
            if (me->_awake == kIOPMNullAssertionID)
                return;
            if (dispatch_time(DISPATCH_TIME_NOW, 0) - me->_lastShown < (uint64_t)kAwakeAfterLastFrame &&
                me->_preventsDisplaySleepDuringVideoPlayback) {
                [me _checkAwake];
                return;
            }
            IOPMAssertionRelease(me->_awake);
            me->_awake = kIOPMNullAssertionID;
        }];
    });
}

/* ---- Status ---- */

- (void)_setStatus:(AVQueuedSampleBufferRenderingStatus)status error:(NSError *)error {
    if (status == _status && error == _error)
        return;
    [self willChangeValueForKey:@"status"];
    if (error != _error)
        [self willChangeValueForKey:@"error"];
    BOOL errorChanged = error != _error;
    _status = status;
    _error = error;
    if (errorChanged)
        [self didChangeValueForKey:@"error"];
    [self didChangeValueForKey:@"status"];
}

/* Locked: decoding failed; the layer takes no more until flushed. */
- (void)_failWith:(OSStatus)status {
    NSError *underlying = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
    NSError *e = [NSError errorWithDomain:AVFoundationErrorDomain code:AVErrorDecodeFailed
                                 userInfo:@{ NSUnderlyingErrorKey: underlying }];
    [self _setStatus:AVQueuedSampleBufferRenderingStatusFailed error:e];
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter]
            postNotificationName:AVSampleBufferDisplayLayerFailedToDecodeNotification object:self
                        userInfo:@{ AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey: e }];
    });
}

- (void)_dropDecoder {
    if (_decoder != NULL) {
        VTDecompressionSessionInvalidate(_decoder);
        CFRelease(_decoder);
        _decoder = NULL;
    }
    if (_decoderFormat != NULL) {
        CFRelease(_decoderFormat);
        _decoderFormat = NULL;
    }
}

/* ---- Flushing ---- */

- (void)flush {
    [self _locked:^{
        [self->_waiting removeAllObjects];
        [self _dropDecoder];
        if (self->_timer != NULL)
            dispatch_source_set_timer(self->_timer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
        [self _setStatus:AVQueuedSampleBufferRenderingStatusUnknown error:nil];
        [self _noteReadiness];
    }];
}

- (void)flushAndRemoveImage {
    [self flush];
    [self _locked:^{
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        self.contents = nil;
        [CATransaction commit];
        for (int i = 0; i < 2; i++) {
            if (self->_held[i] != NULL)
                CVBufferRelease(self->_held[i]);
            self->_held[i] = NULL;
        }
    }];
}

/* ---- Asking for data ---- */

- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block {
    [self _locked:^{
        self->_requestQueue = queue;
        self->_requestBlock = [block copy];
        self->_wasReady = self->_waiting.count < kQueueLimit;
        if (self->_wasReady)
            [self _askForData];
    }];
}

- (void)stopRequestingMediaData {
    [self _locked:^{
        self->_requestQueue = nil;
        self->_requestBlock = nil;
    }];
}

/* Locked: the block runs again each time the layer becomes ready. */
- (void)_noteReadiness {
    BOOL ready = _waiting.count < kQueueLimit;
    if (ready && !_wasReady)
        [self _askForData];
    _wasReady = ready;
}

- (void)_askForData {
    void (^block)(void) = _requestBlock;
    if (block != nil && _requestQueue != nil)
        dispatch_async(_requestQueue, block);
}

@end
