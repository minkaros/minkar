/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A layer that shows video frames given as sample buffers: decoded frames
 * (an image buffer each) or compressed ones, which it decodes; each shown
 * at its presentation time on the control timebase, or at once.
 */

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreMedia/CoreMedia.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVQueuedSampleBufferRenderingStatus) {
    AVQueuedSampleBufferRenderingStatusUnknown,
    AVQueuedSampleBufferRenderingStatusRendering,
    AVQueuedSampleBufferRenderingStatusFailed,
};

typedef NSString *AVLayerVideoGravity NS_STRING_ENUM;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResizeAspect;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResizeAspectFill;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResize;

/* Posted when a compressed sample cannot be decoded; the error is under
 * the key. */
FOUNDATION_EXPORT NSNotificationName const AVSampleBufferDisplayLayerFailedToDecodeNotification;
FOUNDATION_EXPORT NSString *const AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey;

@protocol AVQueuedSampleBufferRendering <NSObject>
@property (readonly) __attribute__((NSObject)) CMTimebaseRef timebase;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
@property (readonly, getter=isReadyForMoreMediaData) BOOL readyForMoreMediaData;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
@property (readonly) BOOL hasSufficientMediaDataForReliablePlaybackStart;
@end

@interface AVSampleBufferDisplayLayer : CALayer <AVQueuedSampleBufferRendering>
@property (copy) AVLayerVideoGravity videoGravity;
/* The timebase the frames' presentation times are read on; without one,
 * every frame is shown as it comes. */
@property (retain, nullable) __attribute__((NSObject)) CMTimebaseRef controlTimebase;
@property (readonly) AVQueuedSampleBufferRenderingStatus status;
@property (readonly, nullable) NSError *error;
@property (readonly, getter=isReadyForMoreMediaData) BOOL readyForMoreMediaData;
@property (readonly) BOOL hasSufficientMediaDataForReliablePlaybackStart;
@property BOOL preventsCapture;
@property BOOL preventsDisplaySleepDuringVideoPlayback;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (void)flushAndRemoveImage;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
@end

NS_ASSUME_NONNULL_END
