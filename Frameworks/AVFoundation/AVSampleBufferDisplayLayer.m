/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AVFoundation/AVSampleBufferDisplayLayer.h>

AVLayerVideoGravity const AVLayerVideoGravityResizeAspect = @"AVLayerVideoGravityResizeAspect";
AVLayerVideoGravity const AVLayerVideoGravityResizeAspectFill = @"AVLayerVideoGravityResizeAspectFill";
AVLayerVideoGravity const AVLayerVideoGravityResize = @"AVLayerVideoGravityResize";

@implementation AVSampleBufferDisplayLayer

- (instancetype)init {
    if ((self = [super init]) != nil)
        _videoGravity = AVLayerVideoGravityResizeAspect;
    return self;
}

- (BOOL)isReadyForMoreMediaData {
    return YES;
}

- (void)enqueueSampleBuffer:(CMSampleBufferRef)sampleBuffer {
    _status = AVQueuedSampleBufferRenderingStatusRendering;
}

- (void)flush {
}

- (void)flushAndRemoveImage {
    self.contents = nil;
}

- (void)requestMediaDataWhenReadyOnQueue:(dispatch_queue_t)queue usingBlock:(void (^)(void))block {
    dispatch_async(queue, block);
}

- (void)stopRequestingMediaData {
}

@end
