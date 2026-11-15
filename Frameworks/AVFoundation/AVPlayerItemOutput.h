/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A player item's video frames for the app, as pixel buffers, the one due
 * at a given time of the item.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CVPixelBuffer.h>

NS_ASSUME_NONNULL_BEGIN

@protocol AVPlayerItemOutputPullDelegate;

@interface AVPlayerItemOutput : NSObject
- (CMTime)itemTimeForHostTime:(CFTimeInterval)hostTimeInSeconds;
- (CMTime)itemTimeForMachAbsoluteTime:(int64_t)machAbsoluteTime;
@property BOOL suppressesPlayerRendering;
@end

@interface AVPlayerItemVideoOutput : AVPlayerItemOutput
- (instancetype)initWithPixelBufferAttributes:(nullable NSDictionary<NSString *, id> *)pixelBufferAttributes;
- (instancetype)initWithOutputSettings:(nullable NSDictionary<NSString *, id> *)outputSettings;
- (BOOL)hasNewPixelBufferForItemTime:(CMTime)itemTime;
- (nullable CVPixelBufferRef)copyPixelBufferForItemTime:(CMTime)itemTime
                                     itemTimeForDisplay:(nullable CMTime *)outItemTimeForDisplay CF_RETURNS_RETAINED;
- (void)setDelegate:(nullable id<AVPlayerItemOutputPullDelegate>)delegate queue:(nullable dispatch_queue_t)delegateQueue;
- (void)requestNotificationOfMediaDataChangeWithAdvanceInterval:(NSTimeInterval)interval;
@property (readonly, weak, nullable) id<AVPlayerItemOutputPullDelegate> delegate;
@property (readonly, nullable) dispatch_queue_t delegateQueue;
@end

@protocol AVPlayerItemOutputPullDelegate <NSObject>
@optional
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;
@end

NS_ASSUME_NONNULL_END
