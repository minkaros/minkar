/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Player items. The asset is loaded and its playback set up (its decoding
 * sessions) away from the main thread; the item is then ready to play,
 * which it says on the main thread.
 */

#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVTime.h>
#import "AVPlayerInternal.h"
#include <mach/mach_time.h>

NSNotificationName const AVPlayerItemTimeJumpedNotification = @"AVPlayerItemTimeJumpedNotification";
NSNotificationName const AVPlayerItemDidPlayToEndTimeNotification = @"AVPlayerItemDidPlayToEndTimeNotification";
NSNotificationName const AVPlayerItemFailedToPlayToEndTimeNotification = @"AVPlayerItemFailedToPlayToEndTimeNotification";
NSNotificationName const AVPlayerItemPlaybackStalledNotification = @"AVPlayerItemPlaybackStalledNotification";
NSNotificationName const AVPlayerItemNewAccessLogEntryNotification = @"AVPlayerItemNewAccessLogEntryNotification";
NSNotificationName const AVPlayerItemNewErrorLogEntryNotification = @"AVPlayerItemNewErrorLogEntryNotification";
NSString *const AVPlayerItemFailedToPlayToEndTimeErrorKey = @"AVPlayerItemFailedToPlayToEndTimeErrorKey";
AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmLowQualityZeroLatency = @"LowQualityZeroLatency";
AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmTimeDomain = @"TimeDomain";
AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmSpectral = @"Spectral";
AVAudioTimePitchAlgorithm const AVAudioTimePitchAlgorithmVarispeed = @"Varispeed";

@interface AVPlayerItemOutput ()
- (void)set_item:(AVPlayerItem *)item;
- (AVPlayerItem *)_item;
@end

@interface AVPlayerItemTrack ()
- (instancetype)_initWithAssetTrack:(AVAssetTrack *)track;
@end

@implementation AVPlayerItemTrack

- (instancetype)_initWithAssetTrack:(AVAssetTrack *)track {
    if ((self = [super init]) != nil) {
        _assetTrack = track;
        _enabled = YES;
    }
    return self;
}

- (float)currentVideoFrameRate {
    return [_assetTrack.mediaType isEqual:AVMediaTypeVideo] ? _assetTrack.nominalFrameRate : 0;
}

@end

@implementation AVPlayerItem {
    AVPlaybackEngine *_engine;
    __weak AVPlayer *_player;
    NSMutableArray<AVPlayerItemOutput *> *_outputs;
    CMTime _pendingSeek;
    BOOL _endPosted;
}

+ (instancetype)playerItemWithURL:(NSURL *)URL {
    return [[self alloc] initWithURL:URL];
}

+ (instancetype)playerItemWithAsset:(AVAsset *)asset {
    return [[self alloc] initWithAsset:asset];
}

+ (instancetype)playerItemWithAsset:(AVAsset *)asset automaticallyLoadedAssetKeys:(NSArray<NSString *> *)keys {
    return [[self alloc] initWithAsset:asset automaticallyLoadedAssetKeys:keys];
}

- (instancetype)initWithURL:(NSURL *)URL {
    return [self initWithAsset:[AVURLAsset URLAssetWithURL:URL options:nil]];
}

- (instancetype)initWithAsset:(AVAsset *)asset {
    return [self initWithAsset:asset automaticallyLoadedAssetKeys:nil];
}

- (instancetype)initWithAsset:(AVAsset *)asset automaticallyLoadedAssetKeys:(NSArray<NSString *> *)keys {
    if ((self = [super init]) == nil)
        return nil;
    _asset = asset;
    _automaticallyLoadedAssetKeys = [keys copy] ?: @[];
    _outputs = [NSMutableArray array];
    _tracks = @[];
    _duration = kCMTimeIndefinite;
    _forwardPlaybackEndTime = kCMTimeInvalid;
    _reversePlaybackEndTime = kCMTimeInvalid;
    _pendingSeek = kCMTimeInvalid;
    _audioTimePitchAlgorithm = AVAudioTimePitchAlgorithmSpectral;
    __weak AVPlayerItem *weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        AVPlaybackEngine *engine = [[AVPlaybackEngine alloc] initWithAsset:asset error:&error];
        dispatch_async(dispatch_get_main_queue(), ^{
            AVPlayerItem *item = weakSelf;
            if (item == nil) {
                [engine invalidate];
                return;
            }
            [item _setUpWithEngine:engine error:error];
        });
    });
    return self;
}

- (void)dealloc {
    [_engine invalidate];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[AVPlayerItem allocWithZone:zone] initWithAsset:_asset automaticallyLoadedAssetKeys:_automaticallyLoadedAssetKeys];
}

- (void)_setUpWithEngine:(AVPlaybackEngine *)engine error:(NSError *)error {
    if (engine == nil) {
        [self willChangeValueForKey:@"error"];
        _error = error;
        [self didChangeValueForKey:@"error"];
        [self willChangeValueForKey:@"status"];
        _status = AVPlayerItemStatusFailed;
        [self didChangeValueForKey:@"status"];
        return;
    }
    _engine = engine;
    if (CMTIME_IS_NUMERIC(_pendingSeek))
        [engine seekToTime:_pendingSeek completionHandler:nil];
    NSMutableArray *tracks = [NSMutableArray array];
    for (AVAssetTrack *t in _asset.tracks)
        [tracks addObject:[[AVPlayerItemTrack alloc] _initWithAssetTrack:t]];
    [self willChangeValueForKey:@"tracks"];
    _tracks = tracks;
    [self didChangeValueForKey:@"tracks"];
    [self willChangeValueForKey:@"duration"];
    _duration = engine.duration;
    [self didChangeValueForKey:@"duration"];
    [self willChangeValueForKey:@"presentationSize"];
    _presentationSize = engine.presentationSize;
    [self didChangeValueForKey:@"presentationSize"];
    [self willChangeValueForKey:@"status"];
    _status = AVPlayerItemStatusReadyToPlay;
    [self didChangeValueForKey:@"status"];
    [_player _itemBecameReady:self];
}

- (AVPlaybackEngine *)_engine {
    return _engine;
}

- (AVPlayer *)_player {
    return _player;
}

- (void)set_player:(AVPlayer *)player {
    _player = player;
}

- (CMTime)currentTime {
    if (_engine != nil)
        return [_engine currentTime];
    return CMTIME_IS_NUMERIC(_pendingSeek) ? _pendingSeek : kCMTimeZero;
}

- (CMTimebaseRef)timebase {
    return _engine.timebase;
}

- (NSArray<NSValue *> *)seekableTimeRanges {
    if (!CMTIME_IS_NUMERIC(_duration))
        return @[];
    return @[ [NSValue valueWithCMTimeRange:CMTimeRangeMake(kCMTimeZero, _duration)] ];
}

/* A local file is all there. */
- (NSArray<NSValue *> *)loadedTimeRanges {
    return self.seekableTimeRanges;
}

- (BOOL)isPlaybackLikelyToKeepUp {
    return _status == AVPlayerItemStatusReadyToPlay;
}

- (BOOL)isPlaybackBufferFull {
    return _status == AVPlayerItemStatusReadyToPlay;
}

- (BOOL)isPlaybackBufferEmpty {
    return _status != AVPlayerItemStatusReadyToPlay;
}

- (BOOL)canPlayFastForward {
    return _engine.hasVideo;
}

- (BOOL)canPlaySlowForward {
    return _engine.hasVideo;
}

- (BOOL)canPlayReverse {
    return NO;
}

- (BOOL)canPlaySlowReverse {
    return NO;
}

- (BOOL)canPlayFastReverse {
    return NO;
}

- (BOOL)canStepForward {
    return _engine.hasVideo;
}

- (BOOL)canStepBackward {
    return _engine.hasVideo;
}

- (BOOL)canUseNetworkResourcesForLiveStreamingWhilePaused {
    return NO;
}

- (void)seekToTime:(CMTime)time completionHandler:(void (^)(BOOL))completionHandler {
    _endPosted = NO;
    if (_engine == nil) {
        _pendingSeek = time;
        if (completionHandler != nil)
            dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(YES); });
        return;
    }
    [_engine seekToTime:time completionHandler:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:AVPlayerItemTimeJumpedNotification object:self];
    [_player _itemTimeJumped:self];
    if (completionHandler != nil)
        dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(YES); });
}

/* The decoder gives each track from the frame or samples covering the
   time: every seek is exact. */
- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)before toleranceAfter:(CMTime)after
    completionHandler:(void (^)(BOOL))completionHandler {
    [self seekToTime:time completionHandler:completionHandler];
}

- (void)seekToTime:(CMTime)time {
    [self seekToTime:time completionHandler:nil];
}

- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)before toleranceAfter:(CMTime)after {
    [self seekToTime:time completionHandler:nil];
}

/* Files carry no dates of their own to seek to. */
- (BOOL)seekToDate:(NSDate *)date completionHandler:(void (^)(BOOL))completionHandler {
    return NO;
}

- (BOOL)seekToDate:(NSDate *)date {
    return NO;
}

- (NSDate *)currentDate {
    return nil;
}

- (void)cancelPendingSeeks {
}

/* A frame forward or back, from the time shown. */
- (void)stepByCount:(NSInteger)count {
    float fps = 0;
    for (AVAssetTrack *t in [_asset tracksWithMediaType:AVMediaTypeVideo])
        fps = t.nominalFrameRate;
    if (fps <= 0 || count == 0)
        return;
    _player.rate = 0;
    CMTime t = CMTimeAdd(self.currentTime, CMTimeMakeWithSeconds(count / fps, 90000));
    [self seekToTime:t];
}

- (NSArray<AVPlayerItemOutput *> *)outputs {
    return [_outputs copy];
}

- (void)addOutput:(AVPlayerItemOutput *)output {
    if (output != nil && ![_outputs containsObject:output]) {
        [_outputs addObject:output];
        [output set_item:self];
    }
}

- (void)removeOutput:(AVPlayerItemOutput *)output {
    [_outputs removeObject:output];
}

/* Played to its end (or its forward end time) once: says so. */
- (BOOL)_reachedEnd {
    if (_engine == nil || _endPosted)
        return NO;
    BOOL end = _engine.isAtEnd;
    if (!end && CMTIME_IS_NUMERIC(_forwardPlaybackEndTime))
        end = CMTimeCompare(_engine.currentTime, _forwardPlaybackEndTime) >= 0;
    if (!end)
        return NO;
    _endPosted = YES;
    NSError *e = _engine.error;
    if (e != nil)
        [[NSNotificationCenter defaultCenter] postNotificationName:AVPlayerItemFailedToPlayToEndTimeNotification
                                                            object:self
                                                          userInfo:@{ AVPlayerItemFailedToPlayToEndTimeErrorKey: e }];
    else
        [[NSNotificationCenter defaultCenter] postNotificationName:AVPlayerItemDidPlayToEndTimeNotification object:self];
    return YES;
}

@end

/* ---- Outputs ------------------------------------------------------------------- */

static double machSeconds(int64_t t) {
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (double)t * tb.numer / tb.denom / 1e9;
}

@implementation AVPlayerItemOutput {
@protected
    __weak AVPlayerItem *_item;
}

/* The item's time at a host time, by its timebase. */
- (CMTime)itemTimeForHostTime:(CFTimeInterval)hostTime {
    AVPlayerItem *item = _item;
    CMTimebaseRef tb = item.timebase;
    if (tb == NULL)
        return kCMTimeInvalid;
    CMTime now = CMClockGetTime(CMClockGetHostTimeClock());
    CMTime then = CMTimeMakeWithSeconds(hostTime, 1000000000);
    double rate = CMTimebaseGetRate(tb);
    return CMTimeAdd(item.currentTime, CMTimeMultiplyByFloat64(CMTimeSubtract(then, now), rate));
}

- (CMTime)itemTimeForMachAbsoluteTime:(int64_t)t {
    return [self itemTimeForHostTime:machSeconds(t)];
}

- (void)set_item:(AVPlayerItem *)item {
    _item = item;
}

- (AVPlayerItem *)_item {
    return _item;
}

@end

@implementation AVPlayerItemVideoOutput {
    NSDictionary *_attributes;
    CMTime _lastCopied;
    __weak id<AVPlayerItemOutputPullDelegate> _delegate;
    dispatch_queue_t _delegateQueue;
}

- (instancetype)initWithPixelBufferAttributes:(NSDictionary *)attributes {
    if ((self = [super init]) != nil) {
        _attributes = [attributes copy];
        _lastCopied = kCMTimeInvalid;
    }
    return self;
}

- (instancetype)initWithOutputSettings:(NSDictionary *)settings {
    return [self initWithPixelBufferAttributes:settings];
}

- (instancetype)init {
    return [self initWithPixelBufferAttributes:nil];
}

- (BOOL)hasNewPixelBufferForItemTime:(CMTime)itemTime {
    CMTime t = kCMTimeInvalid;
    CVPixelBufferRef pb = [[_item _engine] copyFrameAtTime:itemTime frameTime:&t];
    if (pb == NULL)
        return NO;
    CVPixelBufferRelease(pb);
    return !CMTIME_IS_VALID(_lastCopied) || CMTimeCompare(t, _lastCopied) != 0;
}

/* SHORTCUT(AU5): frames are BGRA whatever the attributes ask for. */
- (CVPixelBufferRef)copyPixelBufferForItemTime:(CMTime)itemTime itemTimeForDisplay:(CMTime *)outTime {
    CMTime t = kCMTimeInvalid;
    CVPixelBufferRef pb = [[_item _engine] copyFrameAtTime:itemTime frameTime:&t];
    if (pb != NULL) {
        _lastCopied = t;
        if (outTime != NULL)
            *outTime = t;
    }
    return pb;
}

- (void)setDelegate:(id<AVPlayerItemOutputPullDelegate>)delegate queue:(dispatch_queue_t)queue {
    _delegate = delegate;
    _delegateQueue = queue;
}

- (id<AVPlayerItemOutputPullDelegate>)delegate {
    return _delegate;
}

- (dispatch_queue_t)delegateQueue {
    return _delegateQueue;
}

/* Frames are always at hand once the item is ready: the delegate hears
   so at once. */
- (void)requestNotificationOfMediaDataChangeWithAdvanceInterval:(NSTimeInterval)interval {
    id<AVPlayerItemOutputPullDelegate> d = _delegate;
    if (d == nil || ![d respondsToSelector:@selector(outputMediaDataWillChange:)])
        return;
    dispatch_async(_delegateQueue ?: dispatch_get_main_queue(), ^{
        [d outputMediaDataWillChange:self];
    });
}

@end
