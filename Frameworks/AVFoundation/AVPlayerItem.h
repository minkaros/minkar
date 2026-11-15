/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A player item: an asset's presentation state as a player plays it, its
 * time, its tracks and its outputs.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVAsset.h>

NS_ASSUME_NONNULL_BEGIN

@class AVPlayerItemOutput, AVPlayerItemTrack;

FOUNDATION_EXPORT NSNotificationName const AVPlayerItemTimeJumpedNotification;
FOUNDATION_EXPORT NSNotificationName const AVPlayerItemDidPlayToEndTimeNotification;
FOUNDATION_EXPORT NSNotificationName const AVPlayerItemFailedToPlayToEndTimeNotification;
FOUNDATION_EXPORT NSNotificationName const AVPlayerItemPlaybackStalledNotification;
FOUNDATION_EXPORT NSNotificationName const AVPlayerItemNewAccessLogEntryNotification;
FOUNDATION_EXPORT NSNotificationName const AVPlayerItemNewErrorLogEntryNotification;
FOUNDATION_EXPORT NSString *const AVPlayerItemFailedToPlayToEndTimeErrorKey;

typedef NSString *AVAudioTimePitchAlgorithm;
FOUNDATION_EXPORT AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmLowQualityZeroLatency;
FOUNDATION_EXPORT AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmTimeDomain;
FOUNDATION_EXPORT AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmSpectral;
FOUNDATION_EXPORT AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmVarispeed;

typedef NS_ENUM(NSInteger, AVPlayerItemStatus) {
    AVPlayerItemStatusUnknown = 0,
    AVPlayerItemStatusReadyToPlay = 1,
    AVPlayerItemStatusFailed = 2,
};

@interface AVPlayerItem : NSObject <NSCopying>
+ (instancetype)playerItemWithURL:(NSURL *)URL;
+ (instancetype)playerItemWithAsset:(AVAsset *)asset;
+ (instancetype)playerItemWithAsset:(AVAsset *)asset
       automaticallyLoadedAssetKeys:(nullable NSArray<NSString *> *)automaticallyLoadedAssetKeys;
- (instancetype)initWithURL:(NSURL *)URL;
- (instancetype)initWithAsset:(AVAsset *)asset;
- (instancetype)initWithAsset:(AVAsset *)asset
    automaticallyLoadedAssetKeys:(nullable NSArray<NSString *> *)automaticallyLoadedAssetKeys;

@property (readonly) AVPlayerItemStatus status;
@property (readonly, nullable) NSError *error;
@property (readonly) AVAsset *asset;
@property (readonly) NSArray<AVPlayerItemTrack *> *tracks;
@property (readonly) CMTime duration;
@property (readonly) CGSize presentationSize;
@property (readonly) NSArray<NSString *> *automaticallyLoadedAssetKeys;
@property (readonly) BOOL canPlayFastForward;
@property (readonly) BOOL canPlaySlowForward;
@property (readonly) BOOL canPlayReverse;
@property (readonly) BOOL canPlaySlowReverse;
@property (readonly) BOOL canPlayFastReverse;
@property (readonly) BOOL canStepForward;
@property (readonly) BOOL canStepBackward;
@property (readonly) BOOL canUseNetworkResourcesForLiveStreamingWhilePaused;

- (CMTime)currentTime;
@property CMTime forwardPlaybackEndTime;
@property CMTime reversePlaybackEndTime;
@property (readonly) NSArray<NSValue *> *seekableTimeRanges;
@property (readonly) NSArray<NSValue *> *loadedTimeRanges;
- (void)seekToTime:(CMTime)time completionHandler:(void (^_Nullable)(BOOL finished))completionHandler;
- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)toleranceBefore toleranceAfter:(CMTime)toleranceAfter
    completionHandler:(void (^_Nullable)(BOOL finished))completionHandler;
- (void)seekToTime:(CMTime)time;
- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)toleranceBefore toleranceAfter:(CMTime)toleranceAfter;
- (BOOL)seekToDate:(NSDate *)date completionHandler:(void (^_Nullable)(BOOL finished))completionHandler;
- (BOOL)seekToDate:(NSDate *)date;
- (void)cancelPendingSeeks;
- (nullable NSDate *)currentDate;
- (void)stepByCount:(NSInteger)stepCount;
@property (readonly, nullable) CMTimebaseRef timebase;

@property (readonly, getter=isPlaybackLikelyToKeepUp) BOOL playbackLikelyToKeepUp;
@property (readonly, getter=isPlaybackBufferFull) BOOL playbackBufferFull;
@property (readonly, getter=isPlaybackBufferEmpty) BOOL playbackBufferEmpty;
@property BOOL canUseNetworkResourcesForLiveStreamingWhilePausedSetting;
@property NSTimeInterval preferredForwardBufferDuration;
@property double preferredPeakBitRate;
@property CGSize preferredMaximumResolution;
@property (copy) AVAudioTimePitchAlgorithm audioTimePitchAlgorithm;
@property BOOL seekingWaitsForVideoCompositionRendering;
@property BOOL appliesPerFrameHDRDisplayMetadata;

@property (readonly) NSArray<AVPlayerItemOutput *> *outputs;
- (void)addOutput:(AVPlayerItemOutput *)output;
- (void)removeOutput:(AVPlayerItemOutput *)output;
@end

@interface AVPlayerItemTrack : NSObject
@property (readonly, nullable) AVAssetTrack *assetTrack;
@property (getter=isEnabled) BOOL enabled;
@property (readonly) float currentVideoFrameRate;
@end

NS_ASSUME_NONNULL_END

#import <AVFoundation/AVPlayerItemOutput.h>
