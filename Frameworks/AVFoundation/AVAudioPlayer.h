/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The audio player: a sound file or its bytes, played, paused, looped,
 * with its volume, position and levels.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol AVAudioPlayerDelegate;

@interface AVAudioPlayer : NSObject
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)outError;
- (nullable instancetype)initWithData:(NSData *)data error:(NSError **)outError;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url fileTypeHint:(nullable NSString *)utiString
                                         error:(NSError **)outError;
- (nullable instancetype)initWithData:(NSData *)data fileTypeHint:(nullable NSString *)utiString
                                error:(NSError **)outError;

- (BOOL)prepareToPlay;
- (BOOL)play;
- (BOOL)playAtTime:(NSTimeInterval)time;
- (void)pause;
- (void)stop;

@property (readonly, getter=isPlaying) BOOL playing;
@property (readonly) NSUInteger numberOfChannels;
@property (readonly) NSTimeInterval duration;
@property (weak, nullable) id<AVAudioPlayerDelegate> delegate;
@property (readonly, nullable) NSURL *url;
@property (readonly, nullable) NSData *data;
@property float pan;
@property float volume;
- (void)setVolume:(float)volume fadeDuration:(NSTimeInterval)duration;
@property BOOL enableRate;
@property float rate;
@property NSTimeInterval currentTime;
@property (readonly) NSTimeInterval deviceCurrentTime;
@property NSInteger numberOfLoops;
@property (readonly) NSDictionary<NSString *, id> *settings;
@property (copy, nullable) NSString *currentDevice;

@property (getter=isMeteringEnabled) BOOL meteringEnabled;
- (void)updateMeters;
- (float)peakPowerForChannel:(NSUInteger)channelNumber;
- (float)averagePowerForChannel:(NSUInteger)channelNumber;
@end

@protocol AVAudioPlayerDelegate <NSObject>
@optional
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player successfully:(BOOL)flag;
- (void)audioPlayerDecodeErrorDidOccur:(AVAudioPlayer *)player error:(nullable NSError *)error;
@end

NS_ASSUME_NONNULL_END
