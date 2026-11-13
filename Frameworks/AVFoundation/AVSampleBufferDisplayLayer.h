/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A layer that shows video frames given as sample buffers.
 * SHORTCUT(AU5): frames are taken and released, not shown, until AU5.
 */

#import <QuartzCore/QuartzCore.h>
#import <CoreMedia/CoreMedia.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVQueuedSampleBufferRenderingStatus) {
    AVQueuedSampleBufferRenderingStatusUnknown,
    AVQueuedSampleBufferRenderingStatusRendering,
    AVQueuedSampleBufferRenderingStatusFailed,
};

typedef NSString *AVLayerVideoGravity;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResizeAspect;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResizeAspectFill;
FOUNDATION_EXPORT AVLayerVideoGravity const AVLayerVideoGravityResize;

@interface AVSampleBufferDisplayLayer : CALayer
@property (copy) AVLayerVideoGravity videoGravity;
@property (readonly) AVQueuedSampleBufferRenderingStatus status;
@property (readonly, nullable) NSError *error;
@property (readonly, getter=isReadyForMoreMediaData) BOOL readyForMoreMediaData;
- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer;
- (void)flush;
- (void)flushAndRemoveImage;
- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block;
- (void)stopRequestingMediaData;
@end

NS_ASSUME_NONNULL_END
