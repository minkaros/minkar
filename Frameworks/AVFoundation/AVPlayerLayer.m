/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The player layer: the frame its player shows, as the layer's contents
 * (the frame's IOSurface), placed by the video gravity. The frame shown
 * and the one before stay held, so the decoder does not write over them
 * while they may still be drawn.
 */

#import <AVFoundation/AVPlayerLayer.h>
#import "AVPlayerInternal.h"

static NSString *gravityFor(AVLayerVideoGravity g) {
    if ([g isEqual:AVLayerVideoGravityResizeAspectFill])
        return kCAGravityResizeAspectFill;
    if ([g isEqual:AVLayerVideoGravityResize])
        return kCAGravityResize;
    return kCAGravityResizeAspect;
}

@implementation AVPlayerLayer {
    CVPixelBufferRef _held[2];
}

+ (AVPlayerLayer *)playerLayerWithPlayer:(AVPlayer *)player {
    AVPlayerLayer *l = [[self alloc] init];
    l.player = player;
    return l;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _videoGravity = AVLayerVideoGravityResizeAspect;
        self.contentsGravity = kCAGravityResizeAspect;
    }
    return self;
}

- (void)dealloc {
    [_player _removeLayer:self];
    for (int i = 0; i < 2; i++)
        if (_held[i] != NULL)
            CVPixelBufferRelease(_held[i]);
}

- (void)setPlayer:(AVPlayer *)player {
    if (player == _player)
        return;
    [_player _removeLayer:self];
    [self _clear];
    _player = player;
    [player _addLayer:self];
}

- (void)setVideoGravity:(AVLayerVideoGravity)gravity {
    _videoGravity = [gravity copy] ?: AVLayerVideoGravityResizeAspect;
    self.contentsGravity = gravityFor(_videoGravity);
}

- (void)_showPixelBuffer:(CVPixelBufferRef)buffer {
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(buffer);
    if (surface == NULL)
        return;
    if (_held[1] != NULL)
        CVPixelBufferRelease(_held[1]);
    _held[1] = _held[0];
    _held[0] = CVPixelBufferRetain(buffer);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.contents = (__bridge id)surface;
    [CATransaction commit];
    if (!_readyForDisplay) {
        [self willChangeValueForKey:@"readyForDisplay"];
        _readyForDisplay = YES;
        [self didChangeValueForKey:@"readyForDisplay"];
    }
}

- (void)_clear {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.contents = nil;
    [CATransaction commit];
    for (int i = 0; i < 2; i++) {
        if (_held[i] != NULL)
            CVPixelBufferRelease(_held[i]);
        _held[i] = NULL;
    }
    if (_readyForDisplay) {
        [self willChangeValueForKey:@"readyForDisplay"];
        _readyForDisplay = NO;
        [self didChangeValueForKey:@"readyForDisplay"];
    }
}

/* Where the video is drawn in the bounds, by the gravity. */
- (CGRect)videoRect {
    CGSize size = _player.currentItem.presentationSize;
    CGRect b = self.bounds;
    if (size.width <= 0 || size.height <= 0 || CGRectIsEmpty(b))
        return CGRectZero;
    if ([_videoGravity isEqual:AVLayerVideoGravityResize])
        return b;
    double sx = b.size.width / size.width, sy = b.size.height / size.height;
    double s = [_videoGravity isEqual:AVLayerVideoGravityResizeAspectFill] ? MAX(sx, sy) : MIN(sx, sy);
    double w = size.width * s, h = size.height * s;
    return CGRectMake(b.origin.x + (b.size.width - w) / 2, b.origin.y + (b.size.height - h) / 2, w, h);
}

@end
