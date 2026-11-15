/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A layer showing a player's video.
 */

#import <QuartzCore/QuartzCore.h>
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVSampleBufferDisplayLayer.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVPlayerLayer : CALayer
+ (AVPlayerLayer *)playerLayerWithPlayer:(nullable AVPlayer *)player;
@property (nonatomic, retain, nullable) AVPlayer *player;
@property (copy) AVLayerVideoGravity videoGravity;
@property (nonatomic, readonly, getter=isReadyForDisplay) BOOL readyForDisplay;
@property (nonatomic, readonly) CGRect videoRect;
@property (nonatomic, copy, nullable) NSDictionary<NSString *, id> *pixelBufferAttributes;
@end

NS_ASSUME_NONNULL_END
