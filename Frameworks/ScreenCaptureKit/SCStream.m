/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* Streams: at each frame interval the filter's content is imaged, drawn to
   the configured size into an IOSurface-backed pixel buffer from a pool,
   and handed to the screen outputs as a sample buffer whose attachments
   carry the frame's status, rectangles and scale. Frames whose pixels did
   not change are not sent. Audio is not captured. */

#import <mach/mach_time.h>
#import <stdatomic.h>
#import <IOSurface/IOSurfaceRef.h>
#import "SCInternal.h"

SCStreamFrameInfo const SCStreamFrameInfoStatus = @"SCStreamUpdateFrameStatus";
SCStreamFrameInfo const SCStreamFrameInfoDisplayTime = @"SCStreamUpdateFrameDisplayTime";
SCStreamFrameInfo const SCStreamFrameInfoScaleFactor = @"SCStreamUpdateFrameScaleFactor";
SCStreamFrameInfo const SCStreamFrameInfoContentScale = @"SCStreamUpdateFrameContentScale";
SCStreamFrameInfo const SCStreamFrameInfoContentRect = @"SCStreamUpdateFrameContentRect";
SCStreamFrameInfo const SCStreamFrameInfoDirtyRects = @"SCStreamUpdateFrameDirtyRects";
SCStreamFrameInfo const SCStreamFrameInfoScreenRect = @"SCStreamUpdateFrameScreenRect";
SCStreamFrameInfo const SCStreamFrameInfoBoundingRect = @"SCStreamUpdateFrameBoundingRect";
SCStreamFrameInfo const SCStreamFrameInfoPresenterOverlayContentRect = @"SCStreamUpdateFramePresenterOverlayContentRect";

#pragma mark - Configuration

@implementation SCStreamConfiguration {
    CGColorRef _backgroundColor;
    CFStringRef _colorMatrix;
    CFStringRef _colorSpaceName;
}

@synthesize width = _width, height = _height, minimumFrameInterval = _minimumFrameInterval,
            pixelFormat = _pixelFormat, scalesToFit = _scalesToFit, preservesAspectRatio = _preservesAspectRatio,
            streamName = _streamName, showsCursor = _showsCursor, showMouseClicks = _showMouseClicks,
            sourceRect = _sourceRect, destinationRect = _destinationRect, queueDepth = _queueDepth,
            capturesAudio = _capturesAudio, sampleRate = _sampleRate, channelCount = _channelCount,
            excludesCurrentProcessAudio = _excludesCurrentProcessAudio, captureMicrophone = _captureMicrophone,
            microphoneCaptureDeviceID = _microphoneCaptureDeviceID, ignoreShadowsDisplay = _ignoreShadowsDisplay,
            ignoreShadowsSingleWindow = _ignoreShadowsSingleWindow, ignoreGlobalClipDisplay = _ignoreGlobalClipDisplay,
            ignoreGlobalClipSingleWindow = _ignoreGlobalClipSingleWindow,
            capturesShadowsOnly = _capturesShadowsOnly, shouldBeOpaque = _shouldBeOpaque,
            includeChildWindows = _includeChildWindows, captureResolution = _captureResolution,
            presenterOverlayPrivacyAlertSetting = _presenterOverlayPrivacyAlertSetting;

- (instancetype)init {
    if ((self = [super init])) {
        _minimumFrameInterval = kCMTimeZero;
        _pixelFormat = kCVPixelFormatType_32BGRA;
        _preservesAspectRatio = YES;
        _showsCursor = YES;
        _sourceRect = CGRectNull;
        _destinationRect = CGRectNull;
        _queueDepth = 3;
        _sampleRate = 48000;
        _channelCount = 2;
        _includeChildWindows = YES;
        _captureResolution = SCCaptureResolutionAutomatic;
    }
    return self;
}

- (void)dealloc {
    CGColorRelease(_backgroundColor);
    if (_colorMatrix != NULL)
        CFRelease(_colorMatrix);
    if (_colorSpaceName != NULL)
        CFRelease(_colorSpaceName);
}

- (CGColorRef)backgroundColor {
    return _backgroundColor;
}

- (void)setBackgroundColor:(CGColorRef)color {
    if (color == _backgroundColor)
        return;
    CGColorRetain(color);
    CGColorRelease(_backgroundColor);
    _backgroundColor = color;
}

static void setString(CFStringRef *slot, CFStringRef value) {
    if (*slot == value)
        return;
    if (value != NULL)
        CFRetain(value);
    if (*slot != NULL)
        CFRelease(*slot);
    *slot = value;
}

- (CFStringRef)colorMatrix {
    return _colorMatrix;
}

- (void)setColorMatrix:(CFStringRef)matrix {
    setString(&_colorMatrix, matrix);
}

- (CFStringRef)colorSpaceName {
    return _colorSpaceName;
}

- (void)setColorSpaceName:(CFStringRef)name {
    setString(&_colorSpaceName, name);
}

- (id)copyWithZone:(NSZone *)zone {
    SCStreamConfiguration *c = [[SCStreamConfiguration allocWithZone:zone] init];
    c->_width = _width;
    c->_height = _height;
    c->_minimumFrameInterval = _minimumFrameInterval;
    c->_pixelFormat = _pixelFormat;
    c->_scalesToFit = _scalesToFit;
    c->_preservesAspectRatio = _preservesAspectRatio;
    c->_streamName = [_streamName copy];
    c->_showsCursor = _showsCursor;
    c->_showMouseClicks = _showMouseClicks;
    c.backgroundColor = _backgroundColor;
    c->_sourceRect = _sourceRect;
    c->_destinationRect = _destinationRect;
    c->_queueDepth = _queueDepth;
    c.colorMatrix = _colorMatrix;
    c.colorSpaceName = _colorSpaceName;
    c->_capturesAudio = _capturesAudio;
    c->_sampleRate = _sampleRate;
    c->_channelCount = _channelCount;
    c->_excludesCurrentProcessAudio = _excludesCurrentProcessAudio;
    c->_captureMicrophone = _captureMicrophone;
    c->_microphoneCaptureDeviceID = [_microphoneCaptureDeviceID copy];
    c->_ignoreShadowsDisplay = _ignoreShadowsDisplay;
    c->_ignoreShadowsSingleWindow = _ignoreShadowsSingleWindow;
    c->_ignoreGlobalClipDisplay = _ignoreGlobalClipDisplay;
    c->_ignoreGlobalClipSingleWindow = _ignoreGlobalClipSingleWindow;
    c->_capturesShadowsOnly = _capturesShadowsOnly;
    c->_shouldBeOpaque = _shouldBeOpaque;
    c->_includeChildWindows = _includeChildWindows;
    c->_captureResolution = _captureResolution;
    c->_presenterOverlayPrivacyAlertSetting = _presenterOverlayPrivacyAlertSetting;
    return c;
}

@end

#pragma mark - Stream

__attribute__((visibility("hidden")))
@interface SCStreamOutputEntry : NSObject
@property (weak) id<SCStreamOutput> output;
@property SCStreamOutputType type;
@property (strong) dispatch_queue_t queue;
@end

@implementation SCStreamOutputEntry
@end

static _Atomic(NSInteger) runningStreams;

static CFDictionaryRef rectDictionary(CGRect r) {
    return CGRectCreateDictionaryRepresentation(r);
}

static BOOL rectIsUnset(CGRect r) {
    return CGRectIsNull(r) || CGRectIsEmpty(r) || CGRectIsInfinite(r);
}

@implementation SCStream {
    dispatch_queue_t _queue; /* all of the capture state below is its own */
    dispatch_queue_t _outputQueue;
    SCContentFilter *_filter;
    SCStreamConfiguration *_config;
    __weak id<SCStreamDelegate> _delegate;
    NSMutableArray<SCStreamOutputEntry *> *_outputs; /* @synchronized */
    dispatch_source_t _timer;
    BOOL _running;
    CVPixelBufferPoolRef _pool;
    size_t _poolWidth, _poolHeight;
    OSType _poolFormat;
    CMVideoFormatDescriptionRef _format;
    uint8_t *_previous; /* the last frame sent, to find what changed */
    size_t _previousBytesPerRow, _previousHeight;
}

+ (NSInteger)_runningStreamCount {
    return atomic_load(&runningStreams);
}

- (instancetype)initWithFilter:(SCContentFilter *)contentFilter
                 configuration:(SCStreamConfiguration *)streamConfig
                      delegate:(id<SCStreamDelegate>)delegate {
    if ((self = [super init])) {
        _queue = dispatch_queue_create("org.minkar.ScreenCaptureKit.stream", DISPATCH_QUEUE_SERIAL);
        _outputQueue = dispatch_queue_create("org.minkar.ScreenCaptureKit.output", DISPATCH_QUEUE_SERIAL);
        _filter = contentFilter;
        _config = [streamConfig copy] ?: [[SCStreamConfiguration alloc] init];
        _delegate = delegate;
        _outputs = [NSMutableArray array];
    }
    return self;
}

- (void)dealloc {
    if (_timer != nil)
        dispatch_source_cancel(_timer);
    if (_running)
        atomic_fetch_sub(&runningStreams, 1);
    if (_pool != NULL)
        CVPixelBufferPoolRelease(_pool);
    if (_format != NULL)
        CFRelease(_format);
    free(_previous);
}

- (CMClockRef)synchronizationClock {
    return CMClockGetHostTimeClock();
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<SCStream %p %@ %zux%zu>", self, _filter, _config.width, _config.height];
}

#pragma mark Outputs

- (BOOL)addStreamOutput:(id<SCStreamOutput>)output
                   type:(SCStreamOutputType)type
     sampleHandlerQueue:(dispatch_queue_t)sampleHandlerQueue
                  error:(NSError **)error {
    if (output == nil || ![output respondsToSelector:@selector(stream:didOutputSampleBuffer:ofType:)] ||
        type < SCStreamOutputTypeScreen || type > SCStreamOutputTypeMicrophone) {
        if (error)
            *error = SCMakeError(SCStreamErrorInvalidParameter, @"The stream output is not valid.");
        return NO;
    }
    SCStreamOutputEntry *entry = [[SCStreamOutputEntry alloc] init];
    entry.output = output;
    entry.type = type;
    entry.queue = sampleHandlerQueue;
    @synchronized(_outputs) {
        for (SCStreamOutputEntry *e in _outputs) {
            if (e.output == output && e.type == type) {
                e.queue = sampleHandlerQueue;
                return YES;
            }
        }
        [_outputs addObject:entry];
    }
    return YES;
}

- (BOOL)removeStreamOutput:(id<SCStreamOutput>)output type:(SCStreamOutputType)type error:(NSError **)error {
    @synchronized(_outputs) {
        for (NSUInteger i = 0; i < _outputs.count; i++) {
            SCStreamOutputEntry *e = _outputs[i];
            if (e.output == output && e.type == type) {
                [_outputs removeObjectAtIndex:i];
                return YES;
            }
        }
    }
    if (error)
        *error = SCMakeError(SCStreamErrorInvalidParameter, @"The stream output was not added to the stream.");
    return NO;
}

- (NSArray<SCStreamOutputEntry *> *)_outputsOfType:(SCStreamOutputType)type {
    NSMutableArray *result = [NSMutableArray array];
    @synchronized(_outputs) {
        for (SCStreamOutputEntry *e in _outputs)
            if (e.type == type && e.output != nil)
                [result addObject:e];
    }
    return result;
}

#pragma mark Starting and stopping

static void complete(void (^handler)(NSError *), NSError *error) {
    if (handler == nil)
        return;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      handler(error);
    });
}

static BOOL supportedPixelFormat(OSType format) {
    return format == kCVPixelFormatType_32BGRA || format == kCVPixelFormatType_32ARGB;
}

- (uint64_t)_intervalNanoseconds {
    CMTime t = _config.minimumFrameInterval;
    Float64 seconds = CMTIME_IS_VALID(t) ? CMTimeGetSeconds(t) : 0;
    if (!(seconds > 0))
        seconds = 1.0 / 60;
    if (seconds < 1.0 / 120)
        seconds = 1.0 / 120;
    return (uint64_t)(seconds * NSEC_PER_SEC);
}

- (void)_scheduleTimer {
    if (_timer != nil)
        dispatch_source_cancel(_timer);
    uint64_t interval = [self _intervalNanoseconds];
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, interval / 10);
    __weak SCStream *weakSelf = self;
    dispatch_source_set_event_handler(_timer, ^{
      SCStream *strongSelf = weakSelf;
      @autoreleasepool {
          [strongSelf _captureFrame];
      }
    });
    dispatch_resume(_timer);
}

- (void)_resetFrames {
    free(_previous);
    _previous = NULL;
    _previousBytesPerRow = _previousHeight = 0;
}

- (void)_stopRunning {
    if (!_running)
        return;
    _running = NO;
    atomic_fetch_sub(&runningStreams, 1);
    if (_timer != nil) {
        dispatch_source_cancel(_timer);
        _timer = nil;
    }
    [self _resetFrames];
}

- (void)startCaptureWithCompletionHandler:(void (^)(NSError *))completionHandler {
    dispatch_async(_queue, ^{
      if (self->_running) {
          complete(completionHandler,
                   SCMakeError(SCStreamErrorAttemptToStartStreamState, @"The stream is already capturing."));
          return;
      }
      if (self->_filter == nil) {
          complete(completionHandler, SCMakeError(SCStreamErrorNoCaptureSource, @"The stream has no content filter."));
          return;
      }
      if (!supportedPixelFormat(self->_config.pixelFormat)) {
          complete(completionHandler,
                   SCMakeError(SCStreamErrorInvalidParameter, @"The stream's pixel format is not supported."));
          return;
      }
      if (self->_config.capturesAudio) {
          complete(completionHandler,
                   SCMakeError(SCStreamErrorFailedToStartAudioCapture, @"Audio capture is not available."));
          return;
      }
      if (self->_config.captureMicrophone) {
          complete(completionHandler, SCMakeError(SCStreamErrorFailedToStartMicrophoneCapture,
                                                  @"Microphone capture is not available."));
          return;
      }
      self->_running = YES;
      atomic_fetch_add(&runningStreams, 1);
      [self _resetFrames];
      [self _scheduleTimer];
      complete(completionHandler, nil);
      id<SCStreamDelegate> delegate = self->_delegate;
      if ([delegate respondsToSelector:@selector(streamDidBecomeActive:)])
          [delegate streamDidBecomeActive:self];
    });
}

- (void)stopCaptureWithCompletionHandler:(void (^)(NSError *))completionHandler {
    dispatch_async(_queue, ^{
      if (!self->_running) {
          complete(completionHandler,
                   SCMakeError(SCStreamErrorAttemptToStopStreamState, @"The stream is not capturing."));
          return;
      }
      [self _stopRunning];
      complete(completionHandler, nil);
      id<SCStreamDelegate> delegate = self->_delegate;
      if ([delegate respondsToSelector:@selector(streamDidBecomeInactive:)])
          [delegate streamDidBecomeInactive:self];
    });
}

/* The content went away: the stream stops and tells its delegate. */
- (void)_stopWithError:(NSError *)error {
    [self _stopRunning];
    id<SCStreamDelegate> delegate = _delegate;
    if ([delegate respondsToSelector:@selector(stream:didStopWithError:)])
        [delegate stream:self didStopWithError:error];
}

- (void)updateContentFilter:(SCContentFilter *)contentFilter completionHandler:(void (^)(NSError *))completionHandler {
    if (contentFilter == nil) {
        complete(completionHandler, SCMakeError(SCStreamErrorInvalidParameter, @"The content filter is nil."));
        return;
    }
    dispatch_async(_queue, ^{
      self->_filter = contentFilter;
      [self _resetFrames];
      complete(completionHandler, nil);
    });
}

- (void)updateConfiguration:(SCStreamConfiguration *)streamConfig completionHandler:(void (^)(NSError *))completionHandler {
    SCStreamConfiguration *config = [streamConfig copy];
    if (config == nil || !supportedPixelFormat(config.pixelFormat)) {
        complete(completionHandler,
                 SCMakeError(SCStreamErrorInvalidParameter, @"The stream configuration is not valid."));
        return;
    }
    dispatch_async(_queue, ^{
      BOOL newInterval = CMTimeCompare(config.minimumFrameInterval, self->_config.minimumFrameInterval) != 0;
      self->_config = config;
      [self _resetFrames];
      if (self->_running && newInterval)
          [self _scheduleTimer];
      complete(completionHandler, nil);
    });
}

#pragma mark Frames

- (BOOL)_preparePoolWidth:(size_t)width height:(size_t)height format:(OSType)format {
    if (_pool != NULL && _poolWidth == width && _poolHeight == height && _poolFormat == format)
        return YES;
    if (_pool != NULL) {
        CVPixelBufferPoolRelease(_pool);
        _pool = NULL;
    }
    if (_format != NULL) {
        CFRelease(_format);
        _format = NULL;
    }
    NSDictionary *attributes = @{
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey : @(format),
        (__bridge NSString *)kCVPixelBufferWidthKey : @(width),
        (__bridge NSString *)kCVPixelBufferHeightKey : @(height),
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    };
    if (CVPixelBufferPoolCreate(NULL, NULL, (__bridge CFDictionaryRef)attributes, &_pool) != kCVReturnSuccess) {
        _pool = NULL;
        return NO;
    }
    _poolWidth = width;
    _poolHeight = height;
    _poolFormat = format;
    return YES;
}

/* The rows that changed since the last frame sent, as one rectangle (in the
   frame's pixels, top left origin); empty when none did. Keeps this frame
   for the next one. */
- (CGRect)_changedRectOf:(const uint8_t *)pixels bytesPerRow:(size_t)bytesPerRow width:(size_t)width height:(size_t)height {
    if (_previous == NULL || _previousBytesPerRow != bytesPerRow || _previousHeight != height) {
        free(_previous);
        _previous = malloc(bytesPerRow * height);
        _previousBytesPerRow = bytesPerRow;
        _previousHeight = height;
        if (_previous != NULL)
            memcpy(_previous, pixels, bytesPerRow * height);
        return CGRectMake(0, 0, width, height);
    }
    size_t rowBytes = width * 4;
    size_t first = height, last = 0;
    for (size_t y = 0; y < height; y++) {
        if (memcmp(_previous + y * bytesPerRow, pixels + y * bytesPerRow, rowBytes) != 0) {
            if (first == height)
                first = y;
            last = y;
        }
    }
    if (first == height)
        return CGRectZero;
    memcpy(_previous + first * bytesPerRow, pixels + first * bytesPerRow, (last - first + 1) * bytesPerRow);
    return CGRectMake(0, first, width, last - first + 1);
}

- (void)_captureFrame {
    if (!_running)
        return;
    NSArray<SCStreamOutputEntry *> *outputs = [self _outputsOfType:SCStreamOutputTypeScreen];
    if (outputs.count == 0)
        return;

    SCContentFilter *filter = _filter;
    SCStreamConfiguration *config = _config;
    CGSize contentSize = CGSizeZero;
    BOOL exists = NO;
    CGImageRef content = [filter _copyContentImageWithSize:&contentSize exists:&exists];
    if (!exists) {
        [self _stopWithError:SCMakeError(SCStreamErrorNoCaptureSource, @"The captured content is gone.")];
        return;
    }
    if (content == NULL || contentSize.width < 1 || contentSize.height < 1) {
        if (content != NULL)
            CGImageRelease(content);
        return;
    }

    /* The part of the content captured, in points, and its pixels. */
    CGFloat pointScale = filter.pointPixelScale > 0 ? filter.pointPixelScale : 1;
    CGRect source = CGRectMake(0, 0, contentSize.width, contentSize.height);
    if (!rectIsUnset(config.sourceRect)) {
        CGRect r = CGRectIntersection(config.sourceRect, source);
        if (!rectIsUnset(r))
            source = r;
    }
    CGFloat imageScale = (CGFloat)CGImageGetWidth(content) / contentSize.width;
    if (!CGRectEqualToRect(source, CGRectMake(0, 0, contentSize.width, contentSize.height))) {
        CGRect crop = CGRectMake(source.origin.x * imageScale, source.origin.y * imageScale,
                                 source.size.width * imageScale, source.size.height * imageScale);
        CGImageRef part = CGImageCreateWithImageInRect(content, CGRectIntegral(crop));
        CGImageRelease(content);
        content = part;
        if (content == NULL)
            return;
    }
    CGSize sourcePixels = CGSizeMake(source.size.width * pointScale, source.size.height * pointScale);

    size_t width = config.width > 0 ? config.width : (size_t)ceil(sourcePixels.width);
    size_t height = config.height > 0 ? config.height : (size_t)ceil(sourcePixels.height);
    if (![self _preparePoolWidth:width height:height format:config.pixelFormat]) {
        CGImageRelease(content);
        return;
    }

    /* Where the content goes in the frame (pixels, top left origin). */
    CGRect dest;
    CGFloat contentScale;
    if (!rectIsUnset(config.destinationRect)) {
        dest = config.destinationRect;
        contentScale = MIN(dest.size.width / sourcePixels.width, dest.size.height / sourcePixels.height);
    } else if (config.preservesAspectRatio) {
        CGFloat s = MIN(width / sourcePixels.width, height / sourcePixels.height);
        /* A window smaller than the frame keeps its size unless asked. */
        if (!config.scalesToFit && filter.streamType == SCStreamTypeWindow && s > 1)
            s = 1;
        CGSize size = CGSizeMake(floor(sourcePixels.width * s), floor(sourcePixels.height * s));
        dest = CGRectMake(floor((width - size.width) / 2), floor((height - size.height) / 2), size.width, size.height);
        contentScale = s;
    } else {
        dest = CGRectMake(0, 0, width, height);
        contentScale = MIN(width / sourcePixels.width, height / sourcePixels.height);
    }

    NSDictionary *aux = @{(__bridge NSString *)kCVPixelBufferPoolAllocationThresholdKey : @(MAX(config.queueDepth, 1))};
    CVPixelBufferRef buffer = NULL;
    if (CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(NULL, _pool, (__bridge CFDictionaryRef)aux, &buffer) !=
        kCVReturnSuccess) {
        /* Every buffer is still out (the outputs are behind): drop it. */
        CGImageRelease(content);
        return;
    }

    CGRect changed = CGRectZero;
    CVPixelBufferLockBaseAddress(buffer, 0);
    uint8_t *base = CVPixelBufferGetBaseAddress(buffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer);
    if (base != NULL) {
        uint32_t info = config.pixelFormat == kCVPixelFormatType_32ARGB ?
            (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big) :
            (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(base, width, height, 8, bytesPerRow, space, info);
        CGColorSpaceRelease(space);
        if (context != NULL) {
            CGColorRef background = config.backgroundColor;
            if (background != NULL)
                CGContextSetFillColorWithColor(context, background);
            else
                CGContextSetRGBFillColor(context, 0, 0, 0, 1);
            CGContextFillRect(context, CGRectMake(0, 0, width, height));
            CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
            /* The context's origin is at the bottom left. */
            CGContextDrawImage(context,
                               CGRectMake(dest.origin.x, height - (dest.origin.y + dest.size.height), dest.size.width,
                                          dest.size.height),
                               content);
            CGContextRelease(context);
        }
        changed = [self _changedRectOf:base bytesPerRow:bytesPerRow width:width height:height];
    }
    CVPixelBufferUnlockBaseAddress(buffer, 0);
    CGImageRelease(content);
    if (base == NULL || CGRectIsEmpty(changed)) {
        CVPixelBufferRelease(buffer);
        return;
    }

    if (_format != NULL && !CMVideoFormatDescriptionMatchesImageBuffer(_format, buffer)) {
        CFRelease(_format);
        _format = NULL;
    }
    if (_format == NULL && CMVideoFormatDescriptionCreateForImageBuffer(NULL, buffer, &_format) != 0) {
        _format = NULL;
        CVPixelBufferRelease(buffer);
        return;
    }
    CMSampleTimingInfo timing;
    timing.presentationTimeStamp = CMClockGetTime(CMClockGetHostTimeClock());
    timing.duration = CMTIME_IS_VALID(config.minimumFrameInterval) &&
                              CMTimeCompare(config.minimumFrameInterval, kCMTimeZero) > 0 ?
                          config.minimumFrameInterval :
                          kCMTimeInvalid;
    timing.decodeTimeStamp = kCMTimeInvalid;
    CMSampleBufferRef sample = NULL;
    OSStatus status = CMSampleBufferCreateReadyWithImageBuffer(NULL, buffer, _format, &timing, &sample);
    CVPixelBufferRelease(buffer);
    if (status != 0 || sample == NULL)
        return;

    /* The frame's information, rectangles in the frame's points. */
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, true);
    if (attachments != NULL && CFArrayGetCount(attachments) > 0) {
        CFMutableDictionaryRef frameInfo = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
        CGRect bounding = CGRectMake(dest.origin.x / pointScale, dest.origin.y / pointScale,
                                     dest.size.width / pointScale, dest.size.height / pointScale);
        CGRect dirty = CGRectMake(changed.origin.x / pointScale, changed.origin.y / pointScale,
                                  changed.size.width / pointScale, changed.size.height / pointScale);
        CGRect screen = source;
        if (filter.streamType == SCStreamTypeDisplay) {
            CGRect display = filter.includedDisplays.firstObject.frame;
            screen = CGRectOffset(source, display.origin.x, display.origin.y);
        }
        NSDictionary *values = @{
            SCStreamFrameInfoStatus : @(SCFrameStatusComplete),
            SCStreamFrameInfoDisplayTime : @(mach_absolute_time()),
            SCStreamFrameInfoScaleFactor : @(pointScale),
            SCStreamFrameInfoContentScale : @(contentScale),
            SCStreamFrameInfoContentRect : CFBridgingRelease(rectDictionary(bounding)),
            SCStreamFrameInfoBoundingRect : CFBridgingRelease(rectDictionary(bounding)),
            SCStreamFrameInfoScreenRect : CFBridgingRelease(rectDictionary(screen)),
            SCStreamFrameInfoDirtyRects : @[ CFBridgingRelease(rectDictionary(dirty)) ],
        };
        for (NSString *key in values)
            CFDictionarySetValue(frameInfo, (__bridge CFStringRef)key, (__bridge CFTypeRef)values[key]);
    }

    for (SCStreamOutputEntry *entry in outputs) {
        id<SCStreamOutput> output = entry.output;
        if (output == nil)
            continue;
        CFRetain(sample);
        dispatch_async(entry.queue ?: _outputQueue, ^{
          @autoreleasepool {
              [output stream:self didOutputSampleBuffer:sample ofType:SCStreamOutputTypeScreen];
          }
          CFRelease(sample);
        });
    }
    CFRelease(sample);
}

@end
