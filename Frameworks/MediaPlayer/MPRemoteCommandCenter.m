/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <MediaPlayer/MediaPlayer.h>
#import <objc/message.h>

@interface MPRemoteCommandEvent ()
- (instancetype)_initWithCommand:(MPRemoteCommand *)command;
@end

@interface MPChangePlaybackPositionCommandEvent ()
- (instancetype)_initWithCommand:(MPRemoteCommand *)command positionTime:(NSTimeInterval)positionTime;
@end

@interface MPRemoteCommand ()
- (MPRemoteCommandHandlerStatus)_send:(MPRemoteCommandEvent *)event;
@end

@implementation MPRemoteCommandEvent

- (instancetype)_initWithCommand:(MPRemoteCommand *)command {
    if ((self = [super init]) != nil) {
        _command = command;
        _timestamp = [[NSProcessInfo processInfo] systemUptime];
    }
    return self;
}

@end

@implementation MPChangePlaybackPositionCommandEvent

- (instancetype)_initWithCommand:(MPRemoteCommand *)command positionTime:(NSTimeInterval)positionTime {
    if ((self = [super _initWithCommand:command]) != nil)
        _positionTime = positionTime;
    return self;
}

@end

@implementation MPSkipIntervalCommandEvent
@end

/* A target: a handler, or an object and its action. */
@interface _MPRemoteCommandTarget : NSObject
@property (weak) id object;
@property SEL action;
@property (copy) MPRemoteCommandHandlerStatus (^handler)(MPRemoteCommandEvent *);
@end

@implementation _MPRemoteCommandTarget
@end

@implementation MPRemoteCommand {
    NSMutableArray<_MPRemoteCommandTarget *> *_targets;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _targets = [NSMutableArray array];
        _enabled = YES;
    }
    return self;
}

- (void)addTarget:(id)target action:(SEL)action {
    _MPRemoteCommandTarget *entry = [[_MPRemoteCommandTarget alloc] init];
    entry.object = target;
    entry.action = action;
    @synchronized (self) {
        [_targets addObject:entry];
    }
}

- (void)removeTarget:(id)target action:(SEL)action {
    @synchronized (self) {
        [_targets filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(_MPRemoteCommandTarget *entry, NSDictionary *bindings) {
            return !(entry == target || (entry.object == target && (action == NULL || entry.action == action)));
        }]];
    }
}

- (void)removeTarget:(id)target {
    [self removeTarget:target action:NULL];
}

- (id)addTargetWithHandler:(MPRemoteCommandHandlerStatus (^)(MPRemoteCommandEvent *))handler {
    _MPRemoteCommandTarget *entry = [[_MPRemoteCommandTarget alloc] init];
    entry.handler = handler;
    @synchronized (self) {
        [_targets addObject:entry];
    }
    return entry;
}

- (MPRemoteCommandHandlerStatus)_send:(MPRemoteCommandEvent *)event {
    NSArray *targets;
    @synchronized (self) {
        targets = [_targets copy];
    }
    if (!_enabled || [targets count] == 0)
        return MPRemoteCommandHandlerStatusCommandFailed;
    MPRemoteCommandHandlerStatus result = MPRemoteCommandHandlerStatusSuccess;
    for (_MPRemoteCommandTarget *entry in targets) {
        MPRemoteCommandHandlerStatus status = MPRemoteCommandHandlerStatusSuccess;
        if (entry.handler != nil)
            status = entry.handler(event);
        else if (entry.object != nil && [entry.object respondsToSelector:entry.action])
            status = ((MPRemoteCommandHandlerStatus (*)(id, SEL, id))objc_msgSend)(entry.object, entry.action, event);
        if (status != MPRemoteCommandHandlerStatusSuccess && result == MPRemoteCommandHandlerStatusSuccess)
            result = status;
    }
    return result;
}

@end

@implementation MPChangePlaybackPositionCommand
@end

@implementation MPSkipIntervalCommand
@end

@implementation MPRemoteCommandCenter

+ (MPRemoteCommandCenter *)sharedCommandCenter {
    static MPRemoteCommandCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        center = [[MPRemoteCommandCenter alloc] init];
    });
    return center;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _pauseCommand = [[MPRemoteCommand alloc] init];
        _playCommand = [[MPRemoteCommand alloc] init];
        _stopCommand = [[MPRemoteCommand alloc] init];
        _togglePlayPauseCommand = [[MPRemoteCommand alloc] init];
        _nextTrackCommand = [[MPRemoteCommand alloc] init];
        _previousTrackCommand = [[MPRemoteCommand alloc] init];
        _seekForwardCommand = [[MPRemoteCommand alloc] init];
        _seekBackwardCommand = [[MPRemoteCommand alloc] init];
        _skipForwardCommand = [[MPSkipIntervalCommand alloc] init];
        _skipBackwardCommand = [[MPSkipIntervalCommand alloc] init];
        _changePlaybackPositionCommand = [[MPChangePlaybackPositionCommand alloc] init];
    }
    return self;
}

- (MPRemoteCommandHandlerStatus)_sendCommand:(MPRemoteCommand *)command positionTime:(NSTimeInterval)positionTime {
    MPRemoteCommandEvent *event;
    if ([command isKindOfClass:[MPChangePlaybackPositionCommand class]])
        event = [[MPChangePlaybackPositionCommandEvent alloc] _initWithCommand:command positionTime:positionTime];
    else
        event = [[MPRemoteCommandEvent alloc] _initWithCommand:command];
    return [command _send:event];
}

@end
