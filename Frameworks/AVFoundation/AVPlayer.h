/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The player: plays an item (a queue of them, for AVQueuePlayer), its
 * rate, time, volume and time observers.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVPlayerItem.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVPlayerStatus) {
    AVPlayerStatusUnknown = 0,
    AVPlayerStatusReadyToPlay = 1,
    AVPlayerStatusFailed = 2,
};

typedef NS_ENUM(NSInteger, AVPlayerTimeControlStatus) {
    AVPlayerTimeControlStatusPaused = 0,
    AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate = 1,
    AVPlayerTimeControlStatusPlaying = 2,
};

typedef NS_ENUM(NSInteger, AVPlayerActionAtItemEnd) {
    AVPlayerActionAtItemEndAdvance = 0,
    AVPlayerActionAtItemEndPause = 1,
    AVPlayerActionAtItemEndNone = 2,
};

typedef NSString *AVPlayerWaitingReason;
FOUNDATION_EXPORT AVPlayerWaitingReason const AVPlayerWaitingToMinimizeStallsReason;
FOUNDATION_EXPORT AVPlayerWaitingReason const AVPlayerWaitingWhileEvaluatingBufferingRateReason;
FOUNDATION_EXPORT AVPlayerWaitingReason const AVPlayerWaitingWithNoItemToPlayReason;

@interface AVPlayer : NSObject
+ (instancetype)playerWithURL:(NSURL *)URL;
+ (instancetype)playerWithPlayerItem:(nullable AVPlayerItem *)item;
- (instancetype)initWithURL:(NSURL *)URL;
- (instancetype)initWithPlayerItem:(nullable AVPlayerItem *)item;

@property (readonly) AVPlayerStatus status;
@property (readonly, nullable) NSError *error;
@property (readonly, nullable) AVPlayerItem *currentItem;
- (void)replaceCurrentItemWithPlayerItem:(nullable AVPlayerItem *)item;
@property AVPlayerActionAtItemEnd actionAtItemEnd;

@property float rate;
@property float defaultRate;
- (void)play;
- (void)pause;
- (void)playImmediatelyAtRate:(float)rate;
@property (readonly) AVPlayerTimeControlStatus timeControlStatus;
@property (readonly, nullable) AVPlayerWaitingReason reasonForWaitingToPlay;
@property BOOL automaticallyWaitsToMinimizeStalling;

- (CMTime)currentTime;
- (void)seekToTime:(CMTime)time;
- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)toleranceBefore toleranceAfter:(CMTime)toleranceAfter;
- (void)seekToTime:(CMTime)time completionHandler:(void (^)(BOOL finished))completionHandler;
- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)toleranceBefore toleranceAfter:(CMTime)toleranceAfter
    completionHandler:(void (^)(BOOL finished))completionHandler;
- (void)seekToDate:(NSDate *)date;
- (void)seekToDate:(NSDate *)date completionHandler:(void (^)(BOOL finished))completionHandler;
- (void)setRate:(float)rate time:(CMTime)itemTime atHostTime:(CMTime)hostClockTime;
- (void)prerollAtRate:(float)rate completionHandler:(void (^_Nullable)(BOOL finished))completionHandler;
- (void)cancelPendingPrerolls;
@property (nullable, retain) __attribute__((NSObject)) CMClockRef masterClock;
@property (nullable, retain) __attribute__((NSObject)) CMClockRef sourceClock;

@property float volume;
@property (getter=isMuted) BOOL muted;
@property (copy, nullable) NSString *audioOutputDeviceUniqueID;
@property BOOL preventsDisplaySleepDuringVideoPlayback;
@property BOOL appliesMediaSelectionCriteriaAutomatically;
@property (readonly) BOOL outputObscuredDueToInsufficientExternalProtection;
@property (readonly, getter=isExternalPlaybackActive) BOOL externalPlaybackActive;
@property BOOL allowsExternalPlayback;

- (id)addPeriodicTimeObserverForInterval:(CMTime)interval queue:(nullable dispatch_queue_t)queue
                              usingBlock:(void (^)(CMTime time))block;
- (id)addBoundaryTimeObserverForTimes:(NSArray<NSValue *> *)times queue:(nullable dispatch_queue_t)queue
                           usingBlock:(void (^)(void))block;
- (void)removeTimeObserver:(id)observer;
@end

@interface AVQueuePlayer : AVPlayer
+ (instancetype)queuePlayerWithItems:(NSArray<AVPlayerItem *> *)items;
- (instancetype)initWithItems:(NSArray<AVPlayerItem *> *)items;
- (NSArray<AVPlayerItem *> *)items;
- (void)advanceToNextItem;
- (BOOL)canInsertItem:(AVPlayerItem *)item afterItem:(nullable AVPlayerItem *)afterItem;
- (void)insertItem:(AVPlayerItem *)item afterItem:(nullable AVPlayerItem *)afterItem;
- (void)removeItem:(AVPlayerItem *)item;
- (void)removeAllItems;
@end

NS_ASSUME_NONNULL_END
