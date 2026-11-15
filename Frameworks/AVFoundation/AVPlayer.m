/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The player. While it has an item, a timer on the main thread at the
 * display's pace shows the frame due on its layers, runs the time
 * observers and acts at the item's end. Items are played through their
 * playback engines (AVPlaybackEngine).
 */

#import <AVFoundation/AVPlayer.h>
#import "AVPlayerInternal.h"

AVPlayerWaitingReason const AVPlayerWaitingToMinimizeStallsReason = @"AVPlayerWaitingToMinimizeStallsReason";
AVPlayerWaitingReason const AVPlayerWaitingWhileEvaluatingBufferingRateReason =
    @"AVPlayerWaitingWhileEvaluatingBufferingRateReason";
AVPlayerWaitingReason const AVPlayerWaitingWithNoItemToPlayReason = @"AVPlayerWaitingWithNoItemToPlayReason";

#define TICK_HZ 120

@interface AVPlayerTimeObserver : NSObject
@property CMTime interval;		/* periodic, or invalid */
@property (copy) NSArray<NSValue *> *times;	/* boundary */
@property (strong) dispatch_queue_t queue;
@property (copy) void (^periodic)(CMTime);
@property (copy) void (^boundary)(void);
@property double lastFired;		/* item seconds, for periodic ones */
@end

@implementation AVPlayerTimeObserver
@end

@interface AVPlayer ()
- (void)_setItem:(AVPlayerItem *)item;
- (void)_advancePastItem:(AVPlayerItem *)item;
@end

@implementation AVPlayer {
@protected
    AVPlayerItem *_currentItem;
    float _rate;
    NSHashTable<AVPlayerLayer *> *_layers;
    NSMutableArray<AVPlayerTimeObserver *> *_observers;
    dispatch_source_t _timer;
    double _lastTickTime;
    CVPixelBufferRef _shown;
    CMTime _shownTime;
}

+ (instancetype)playerWithURL:(NSURL *)URL {
    return [[self alloc] initWithURL:URL];
}

+ (instancetype)playerWithPlayerItem:(AVPlayerItem *)item {
    return [[self alloc] initWithPlayerItem:item];
}

- (instancetype)init {
    return [self initWithPlayerItem:nil];
}

- (instancetype)initWithURL:(NSURL *)URL {
    return [self initWithPlayerItem:[AVPlayerItem playerItemWithURL:URL]];
}

- (instancetype)initWithPlayerItem:(AVPlayerItem *)item {
    if ((self = [super init]) == nil)
        return nil;
    _layers = [NSHashTable weakObjectsHashTable];
    _observers = [NSMutableArray array];
    _defaultRate = 1;
    _volume = 1;
    _actionAtItemEnd = AVPlayerActionAtItemEndPause;
    _automaticallyWaitsToMinimizeStalling = YES;
    _status = AVPlayerStatusReadyToPlay;
    _shownTime = kCMTimeInvalid;
    [self _setItem:item];
    return self;
}

- (void)dealloc {
    if (_timer != nil)
        dispatch_source_cancel(_timer);
    if (_shown != NULL)
        CVPixelBufferRelease(_shown);
    [_currentItem._engine setRate:0];
    _currentItem._player = nil;
}

/* ---- The item ----------------------------------------------------------------- */

- (AVPlayerItem *)currentItem {
    return _currentItem;
}

- (void)_setItem:(AVPlayerItem *)item {
    if (item == _currentItem)
        return;
    [_currentItem._engine setRate:0];
    _currentItem._player = nil;
    if (_shown != NULL)
        CVPixelBufferRelease(_shown);
    _shown = NULL;
    _shownTime = kCMTimeInvalid;
    for (AVPlayerLayer *l in _layers)
        [l _clear];
    [self willChangeValueForKey:@"currentItem"];
    _currentItem = item;
    [self didChangeValueForKey:@"currentItem"];
    item._player = self;
    if (item._engine != nil)
        [self _itemBecameReady:item];
    [self _updateTimeControlStatus];
    [self _updateTimer];
}

- (void)replaceCurrentItemWithPlayerItem:(AVPlayerItem *)item {
    [self _setItem:item];
}

- (void)_itemBecameReady:(AVPlayerItem *)item {
    if (item != _currentItem)
        return;
    AVPlaybackEngine *engine = item._engine;
    engine.volume = _muted ? 0 : _volume;
    engine.rate = _rate;
    [self _updateTimeControlStatus];
    [self _tick];
}

- (void)_itemTimeJumped:(AVPlayerItem *)item {
    if (_shown != NULL)
        CVPixelBufferRelease(_shown);
    _shown = NULL;
    _shownTime = kCMTimeInvalid;
    _lastTickTime = CMTimeGetSeconds(item.currentTime);
    [self _firePeriodicObservers:YES];
    [self _tick];
}

/* ---- Rate --------------------------------------------------------------------- */

- (float)rate {
    return _rate;
}

- (void)setRate:(float)rate {
    if (rate < 0)
        rate = 0;	/* reverse play: not supported */
    if (rate == _rate)
        return;
    [self willChangeValueForKey:@"rate"];
    _rate = rate;
    [self didChangeValueForKey:@"rate"];
    _currentItem._engine.rate = rate;
    [self _updateTimeControlStatus];
    [self _updateTimer];
    [self _firePeriodicObservers:YES];
}

- (void)play {
    self.rate = _defaultRate > 0 ? _defaultRate : 1;
}

- (void)pause {
    self.rate = 0;
}

- (void)playImmediatelyAtRate:(float)rate {
    self.rate = rate;
}

- (void)setRate:(float)rate time:(CMTime)itemTime atHostTime:(CMTime)hostClockTime {
    if (CMTIME_IS_NUMERIC(itemTime))
        [_currentItem seekToTime:itemTime];
    self.rate = rate;
}

- (void)prerollAtRate:(float)rate completionHandler:(void (^)(BOOL))completionHandler {
    if (completionHandler != nil)
        dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(YES); });
}

- (void)cancelPendingPrerolls {
}

- (void)_updateTimeControlStatus {
    AVPlayerTimeControlStatus s = AVPlayerTimeControlStatusPaused;
    AVPlayerWaitingReason reason = nil;
    if (_rate != 0) {
        if (_currentItem == nil) {
            s = AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate;
            reason = AVPlayerWaitingWithNoItemToPlayReason;
        } else if (_currentItem._engine == nil) {
            s = AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate;
            reason = AVPlayerWaitingWhileEvaluatingBufferingRateReason;
        } else
            s = AVPlayerTimeControlStatusPlaying;
    }
    if (s != _timeControlStatus) {
        [self willChangeValueForKey:@"timeControlStatus"];
        _timeControlStatus = s;
        [self didChangeValueForKey:@"timeControlStatus"];
    }
    if (reason != _reasonForWaitingToPlay) {
        [self willChangeValueForKey:@"reasonForWaitingToPlay"];
        _reasonForWaitingToPlay = reason;
        [self didChangeValueForKey:@"reasonForWaitingToPlay"];
    }
}

/* ---- Time ---------------------------------------------------------------------- */

- (CMTime)currentTime {
    return _currentItem != nil ? _currentItem.currentTime : kCMTimeZero;
}

- (void)seekToTime:(CMTime)time {
    [_currentItem seekToTime:time];
}

- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)before toleranceAfter:(CMTime)after {
    [_currentItem seekToTime:time];
}

- (void)seekToTime:(CMTime)time completionHandler:(void (^)(BOOL))completionHandler {
    if (_currentItem == nil) {
        if (completionHandler != nil)
            dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(NO); });
        return;
    }
    [_currentItem seekToTime:time completionHandler:completionHandler];
}

- (void)seekToTime:(CMTime)time toleranceBefore:(CMTime)before toleranceAfter:(CMTime)after
    completionHandler:(void (^)(BOOL))completionHandler {
    [self seekToTime:time completionHandler:completionHandler];
}

- (void)seekToDate:(NSDate *)date {
}

- (void)seekToDate:(NSDate *)date completionHandler:(void (^)(BOOL))completionHandler {
    if (completionHandler != nil)
        dispatch_async(dispatch_get_main_queue(), ^{ completionHandler(NO); });
}

/* ---- Volume -------------------------------------------------------------------- */

- (void)setVolume:(float)volume {
    _volume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    _currentItem._engine.volume = _muted ? 0 : _volume;
}

- (void)setMuted:(BOOL)muted {
    _muted = muted;
    _currentItem._engine.volume = muted ? 0 : _volume;
}

- (BOOL)outputObscuredDueToInsufficientExternalProtection {
    return NO;
}

- (BOOL)isExternalPlaybackActive {
    return NO;
}

/* ---- Time observers ------------------------------------------------------------ */

- (id)addPeriodicTimeObserverForInterval:(CMTime)interval queue:(dispatch_queue_t)queue
                              usingBlock:(void (^)(CMTime))block {
    AVPlayerTimeObserver *o = [[AVPlayerTimeObserver alloc] init];
    o.interval = CMTIME_IS_NUMERIC(interval) && CMTimeGetSeconds(interval) > 0 ? interval : CMTimeMake(1, 10);
    o.queue = queue ?: dispatch_get_main_queue();
    o.periodic = block;
    o.lastFired = -INFINITY;
    [_observers addObject:o];
    [self _updateTimer];
    return o;
}

- (id)addBoundaryTimeObserverForTimes:(NSArray<NSValue *> *)times queue:(dispatch_queue_t)queue
                           usingBlock:(void (^)(void))block {
    AVPlayerTimeObserver *o = [[AVPlayerTimeObserver alloc] init];
    o.interval = kCMTimeInvalid;
    o.times = times;
    o.queue = queue ?: dispatch_get_main_queue();
    o.boundary = block;
    [_observers addObject:o];
    return o;
}

- (void)removeTimeObserver:(id)observer {
    [_observers removeObject:observer];
}

/* The periodic observers: each when its interval has passed in item time
   (always, when forced: at a start, a stop or a jump). */
- (void)_firePeriodicObservers:(BOOL)force {
    if (_currentItem == nil)
        return;
    CMTime now = self.currentTime;
    double t = CMTimeGetSeconds(now);
    for (AVPlayerTimeObserver *o in [_observers copy]) {
        if (o.periodic == nil)
            continue;
        double interval = CMTimeGetSeconds(o.interval);
        if (!force && fabs(t - o.lastFired) < interval)
            continue;
        /* On the interval's own grid, as the observer asked. */
        o.lastFired = force ? t : floor(t / interval) * interval;
        void (^block)(CMTime) = o.periodic;
        dispatch_async(o.queue, ^{ block(now); });
    }
}

/* The boundary observers whose times were crossed since the last tick. */
- (void)_fireBoundaryObservers:(double)from to:(double)to {
    if (to <= from)
        return;
    for (AVPlayerTimeObserver *o in [_observers copy]) {
        if (o.boundary == nil)
            continue;
        for (NSValue *v in o.times) {
            CMTime bt;
            [v getValue:&bt];
            double b = CMTimeGetSeconds(bt);
            if (b > from && b <= to) {
                dispatch_async(o.queue, o.boundary);
                break;
            }
        }
    }
}

/* ---- Layers and the timer ------------------------------------------------------ */

- (void)_addLayer:(AVPlayerLayer *)layer {
    [_layers addObject:layer];
    if (_shown != NULL)
        [layer _showPixelBuffer:_shown];
    [self _updateTimer];
    [self _tick];
}

- (void)_removeLayer:(AVPlayerLayer *)layer {
    [_layers removeObject:layer];
}

/* The timer runs while there is something to show or to observe. */
- (void)_updateTimer {
    BOOL want = _currentItem != nil;
    if (want && _timer == nil) {
        _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / TICK_HZ, NSEC_PER_SEC / 1000);
        __weak AVPlayer *weakSelf = self;
        dispatch_source_set_event_handler(_timer, ^{ [weakSelf _tick]; });
        dispatch_resume(_timer);
    } else if (!want && _timer != nil) {
        dispatch_source_cancel(_timer);
        _timer = nil;
    }
}

- (void)_tick {
    AVPlayerItem *item = _currentItem;
    AVPlaybackEngine *engine = item._engine;
    if (engine == nil)
        return;
    double now = CMTimeGetSeconds(engine.currentTime);
    if (engine.hasVideo) {
        CMTime t = kCMTimeInvalid;
        CVPixelBufferRef pb = [engine copyFrameForTime:engine.currentTime frameTime:&t];
        if (pb != NULL && (!CMTIME_IS_VALID(_shownTime) || CMTimeCompare(t, _shownTime) != 0)) {
            if (_shown != NULL)
                CVPixelBufferRelease(_shown);
            _shown = CVPixelBufferRetain(pb);
            _shownTime = t;
            for (AVPlayerLayer *l in _layers)
                [l _showPixelBuffer:pb];
        }
        if (pb != NULL)
            CVPixelBufferRelease(pb);
    }
    if (_rate == 0) {
        _lastTickTime = now;
        return;
    }
    [self _fireBoundaryObservers:_lastTickTime to:now];
    _lastTickTime = now;
    [self _firePeriodicObservers:NO];
    if ([item _reachedEnd])
        [self _itemDidReachEnd:item];
}

- (void)_itemDidReachEnd:(AVPlayerItem *)item {
    switch (_actionAtItemEnd) {
    case AVPlayerActionAtItemEndPause:
        self.rate = 0;
        break;
    case AVPlayerActionAtItemEndAdvance:
        [self _advancePastItem:item];
        break;
    case AVPlayerActionAtItemEndNone:
        break;
    }
}

/* A plain player has no next item. */
- (void)_advancePastItem:(AVPlayerItem *)item {
    [self _setItem:nil];
}

@end

/* ---- The queue player ------------------------------------------------------------ */

@implementation AVQueuePlayer {
    NSMutableArray<AVPlayerItem *> *_items;
}

+ (instancetype)queuePlayerWithItems:(NSArray<AVPlayerItem *> *)items {
    return [[self alloc] initWithItems:items];
}

- (instancetype)initWithItems:(NSArray<AVPlayerItem *> *)items {
    if ((self = [super initWithPlayerItem:items.firstObject]) != nil) {
        _items = [items mutableCopy] ?: [NSMutableArray array];
        self.actionAtItemEnd = AVPlayerActionAtItemEndAdvance;
    }
    return self;
}

- (instancetype)initWithPlayerItem:(AVPlayerItem *)item {
    if ((self = [super initWithPlayerItem:item]) != nil) {
        if (_items == nil)
            _items = item != nil ? [NSMutableArray arrayWithObject:item] : [NSMutableArray array];
        self.actionAtItemEnd = AVPlayerActionAtItemEndAdvance;
    }
    return self;
}

- (NSArray<AVPlayerItem *> *)items {
    return [_items copy];
}

- (void)advanceToNextItem {
    AVPlayerItem *current = self.currentItem;
    if (current != nil)
        [_items removeObject:current];
    [self _setItem:_items.firstObject];
}

- (void)_advancePastItem:(AVPlayerItem *)item {
    [self advanceToNextItem];
}

- (BOOL)canInsertItem:(AVPlayerItem *)item afterItem:(AVPlayerItem *)afterItem {
    return item != nil && ![_items containsObject:item] && (afterItem == nil || [_items containsObject:afterItem]);
}

- (void)insertItem:(AVPlayerItem *)item afterItem:(AVPlayerItem *)afterItem {
    if (![self canInsertItem:item afterItem:afterItem])
        return;
    NSUInteger at = afterItem != nil ? [_items indexOfObject:afterItem] + 1 : _items.count;
    [self willChangeValueForKey:@"items"];
    [_items insertObject:item atIndex:at];
    [self didChangeValueForKey:@"items"];
    if (self.currentItem == nil)
        [self _setItem:_items.firstObject];
}

- (void)removeItem:(AVPlayerItem *)item {
    BOOL current = item == self.currentItem;
    [self willChangeValueForKey:@"items"];
    [_items removeObject:item];
    [self didChangeValueForKey:@"items"];
    if (current)
        [self _setItem:_items.firstObject];
}

- (void)removeAllItems {
    [self willChangeValueForKey:@"items"];
    [_items removeAllObjects];
    [self didChangeValueForKey:@"items"];
    [self _setItem:nil];
}

@end
