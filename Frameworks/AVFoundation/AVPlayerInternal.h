/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What the player, its items, layers and outputs share. Private.
 */

#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerLayer.h>
#import "AVPlaybackEngine.h"

@interface AVPlayerItem (Internal)
@property (weak) AVPlayer *_player;
- (AVPlaybackEngine *)_engine;
/* Called by the player on the main thread, each tick it plays the item. */
- (BOOL)_reachedEnd;
@end

@interface AVPlayer (Internal)
- (void)_addLayer:(AVPlayerLayer *)layer;
- (void)_removeLayer:(AVPlayerLayer *)layer;
- (void)_itemBecameReady:(AVPlayerItem *)item;
- (void)_itemTimeJumped:(AVPlayerItem *)item;
@end

@interface AVPlayerLayer (Internal)
- (void)_showPixelBuffer:(CVPixelBufferRef)buffer;
- (void)_clear;
@end
