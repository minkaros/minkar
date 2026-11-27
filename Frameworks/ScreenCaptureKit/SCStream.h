/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* Streams of a display's or a window's frames: what a stream captures (its
   content filter), how (its configuration), and the stream itself with
   its outputs and delegate. */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ScreenCaptureKit/SCShareableContent.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SCStreamOutputType) {
    SCStreamOutputTypeScreen = 0,
    SCStreamOutputTypeAudio = 1,
    SCStreamOutputTypeMicrophone = 2,
};

typedef NS_ENUM(NSInteger, SCFrameStatus) {
    SCFrameStatusComplete = 0,
    SCFrameStatusIdle = 1,
    SCFrameStatusBlank = 2,
    SCFrameStatusSuspended = 3,
    SCFrameStatusStarted = 4,
    SCFrameStatusStopped = 5,
};

typedef NS_ENUM(NSInteger, SCStreamType) {
    SCStreamTypeWindow = 0,
    SCStreamTypeDisplay = 1,
};

typedef NS_ENUM(NSInteger, SCCaptureResolutionType) {
    SCCaptureResolutionAutomatic = 0,
    SCCaptureResolutionBest = 1,
    SCCaptureResolutionNominal = 2,
};

typedef NS_ENUM(NSInteger, SCPresenterOverlayAlertSetting) {
    SCPresenterOverlayAlertSettingSystem = 0,
    SCPresenterOverlayAlertSettingNever = 1,
    SCPresenterOverlayAlertSettingAlways = 2,
};

/* The keys of a frame's attachments (CMSampleBufferGetSampleAttachmentsArray). */
typedef NSString *SCStreamFrameInfo NS_STRING_ENUM;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoStatus;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoDisplayTime;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoScaleFactor;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoContentScale;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoContentRect;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoDirtyRects;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoScreenRect;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoBoundingRect;
FOUNDATION_EXPORT SCStreamFrameInfo const SCStreamFrameInfoPresenterOverlayContentRect;

/* What a stream captures: one window on its own, or a display with some of
   its windows or applications left out or kept. */
@interface SCContentFilter : NSObject
@property (readonly) SCStreamType streamType;
@property (readonly) SCShareableContentStyle style;
/* Pixels per point of the content's display. */
@property (readonly) float pointPixelScale;
/* The content's size, in points, at the origin. */
@property (readonly) CGRect contentRect;
/* Whether a display's capture keeps the menu bar. */
@property BOOL includeMenuBar;
@property (readonly) NSArray<SCDisplay *> *includedDisplays;
@property (readonly) NSArray<SCRunningApplication *> *includedApplications;
@property (readonly) NSArray<SCWindow *> *includedWindows;

- (instancetype)initWithDesktopIndependentWindow:(SCWindow *)window;
- (instancetype)initWithDisplay:(SCDisplay *)display excludingWindows:(NSArray<SCWindow *> *)excluded;
- (instancetype)initWithDisplay:(SCDisplay *)display includingWindows:(NSArray<SCWindow *> *)includedWindows;
- (instancetype)initWithDisplay:(SCDisplay *)display
           includingApplications:(NSArray<SCRunningApplication *> *)applications
                exceptingWindows:(NSArray<SCWindow *> *)exceptingWindows;
- (instancetype)initWithDisplay:(SCDisplay *)display
           excludingApplications:(NSArray<SCRunningApplication *> *)applications
                exceptingWindows:(NSArray<SCWindow *> *)exceptingWindows;
@end

/* How a stream captures: the frames' size, rate, pixel format and the
   part of the content they show. */
@interface SCStreamConfiguration : NSObject <NSCopying>
@property size_t width;
@property size_t height;
/* The shortest time between two frames; zero, as fast as the display. */
@property CMTime minimumFrameInterval;
/* kCVPixelFormatType_32BGRA (the default) or kCVPixelFormatType_32ARGB. */
@property OSType pixelFormat;
@property BOOL scalesToFit;
@property BOOL preservesAspectRatio;
@property (copy, nullable) NSString *streamName;
@property BOOL showsCursor;
@property BOOL showMouseClicks;
/* What shows where there is no content; black by default. */
@property (nullable) CGColorRef backgroundColor;
/* The part of the content captured, in points; empty, all of it. */
@property CGRect sourceRect;
/* Where the content goes in the frame, in points; empty, fit to it. */
@property CGRect destinationRect;
/* How many frames may be out at once (3 to 8). */
@property NSInteger queueDepth;
@property (nullable) CFStringRef colorMatrix;
@property (nullable) CFStringRef colorSpaceName;
@property BOOL capturesAudio;
@property NSInteger sampleRate;
@property NSInteger channelCount;
@property BOOL excludesCurrentProcessAudio;
@property BOOL captureMicrophone;
@property (copy, nullable) NSString *microphoneCaptureDeviceID;
@property BOOL ignoreShadowsDisplay;
@property BOOL ignoreShadowsSingleWindow;
@property BOOL ignoreGlobalClipDisplay;
@property BOOL ignoreGlobalClipSingleWindow;
@property BOOL capturesShadowsOnly;
@property BOOL shouldBeOpaque;
@property BOOL includeChildWindows;
@property SCCaptureResolutionType captureResolution;
@property SCPresenterOverlayAlertSetting presenterOverlayPrivacyAlertSetting;
@end

@class SCStream;

@protocol SCStreamOutput <NSObject>
@optional
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type;
@end

@protocol SCStreamDelegate <NSObject>
@optional
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
- (void)userDidStopStream:(SCStream *)stream;
- (void)outputVideoEffectDidStartForStream:(SCStream *)stream;
- (void)outputVideoEffectDidStopForStream:(SCStream *)stream;
- (void)streamDidBecomeActive:(SCStream *)stream;
- (void)streamDidBecomeInactive:(SCStream *)stream;
@end

@interface SCStream : NSObject
/* The clock of the frames' presentation times (the host's). */
@property (readonly, nullable) CMClockRef synchronizationClock;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithFilter:(SCContentFilter *)contentFilter
                 configuration:(SCStreamConfiguration *)streamConfig
                      delegate:(nullable id<SCStreamDelegate>)delegate NS_DESIGNATED_INITIALIZER;

/* A nil queue delivers on a serial queue of the stream's. */
- (BOOL)addStreamOutput:(id<SCStreamOutput>)output
                   type:(SCStreamOutputType)type
     sampleHandlerQueue:(nullable dispatch_queue_t)sampleHandlerQueue
                  error:(NSError **)error;
- (BOOL)removeStreamOutput:(id<SCStreamOutput>)output type:(SCStreamOutputType)type error:(NSError **)error;

- (void)updateContentFilter:(SCContentFilter *)contentFilter
          completionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;
- (void)updateConfiguration:(SCStreamConfiguration *)streamConfig
          completionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;
- (void)startCaptureWithCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;
- (void)stopCaptureWithCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;
@end

NS_ASSUME_NONNULL_END
