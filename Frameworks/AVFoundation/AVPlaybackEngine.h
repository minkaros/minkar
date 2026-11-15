/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A player item's playback: its video decoded ahead into a short queue of
 * frames, its audio played by an audio queue whose time (the sample being
 * heard) is the clock; with no audio, or at a rate other than 1, the host
 * clock is. Private to AVFoundation.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CVPixelBuffer.h>

@class AVAsset;

@interface AVPlaybackEngine : NSObject
- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError;
@property (readonly) BOOL hasVideo;
@property (readonly) BOOL hasAudio;
@property (readonly) CGSize presentationSize;
@property (readonly) CMTime duration;
@property (readonly) CMTimebaseRef timebase;

- (CMTime)currentTime;
@property (nonatomic) float rate;		/* 0: paused */
@property (nonatomic) float volume;		/* 0 when muted */
/* The item's time jumps there: each track from the frame or samples that
   cover it. */
- (void)seekToTime:(CMTime)time completionHandler:(void (^)(BOOL finished))handler;
/* Played to the end: every track's last sample shown or heard. */
@property (readonly, getter=isAtEnd) BOOL atEnd;
@property (readonly) NSError *error;

/* The latest frame due at the time (frames before it are let go), or NULL
   while none is; its time in *frameTime. */
- (CVPixelBufferRef)copyFrameForTime:(CMTime)time frameTime:(CMTime *)frameTime CF_RETURNS_RETAINED;
/* The frame due at the time without letting any go. */
- (CVPixelBufferRef)copyFrameAtTime:(CMTime)time frameTime:(CMTime *)frameTime CF_RETURNS_RETAINED;
/* The audio's output channels; with metering, each channel's peak and
   mean square since the last call (both 0..1), taken and reset. */
@property (readonly) int audioChannels;
@property (nonatomic) BOOL metering;
- (void)takeLevelsPeak:(float *)peak meanSquare:(float *)meanSquare count:(int)count;
- (void)invalidate;
@end
