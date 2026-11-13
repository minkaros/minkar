/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MPRemoteCommandHandlerStatus) {
    MPRemoteCommandHandlerStatusSuccess = 0,
    MPRemoteCommandHandlerStatusNoSuchContent = 100,
    MPRemoteCommandHandlerStatusNoActionableNowPlayingItem = 110,
    MPRemoteCommandHandlerStatusDeviceNotFound = 120,
    MPRemoteCommandHandlerStatusCommandFailed = 200,
};

@class MPRemoteCommand;

@interface MPRemoteCommandEvent : NSObject
@property (readonly) MPRemoteCommand *command;
@property (readonly) NSTimeInterval timestamp;
@end

@interface MPChangePlaybackPositionCommandEvent : MPRemoteCommandEvent
@property (readonly) NSTimeInterval positionTime;
@end

@interface MPSkipIntervalCommandEvent : MPRemoteCommandEvent
@property (readonly) NSTimeInterval interval;
@end

@interface MPRemoteCommand : NSObject
@property (getter=isEnabled) BOOL enabled;
- (void)addTarget:(id)target action:(SEL)action;
- (void)removeTarget:(id)target action:(nullable SEL)action;
- (void)removeTarget:(nullable id)target;
- (id)addTargetWithHandler:(MPRemoteCommandHandlerStatus (^)(MPRemoteCommandEvent *event))handler;
@end

@interface MPChangePlaybackPositionCommand : MPRemoteCommand
@end

@interface MPSkipIntervalCommand : MPRemoteCommand
@property (copy) NSArray<NSNumber *> *preferredIntervals;
@end

@interface MPRemoteCommandCenter : NSObject
+ (MPRemoteCommandCenter *)sharedCommandCenter;
@property (readonly) MPRemoteCommand *pauseCommand;
@property (readonly) MPRemoteCommand *playCommand;
@property (readonly) MPRemoteCommand *stopCommand;
@property (readonly) MPRemoteCommand *togglePlayPauseCommand;
@property (readonly) MPRemoteCommand *nextTrackCommand;
@property (readonly) MPRemoteCommand *previousTrackCommand;
@property (readonly) MPRemoteCommand *seekForwardCommand;
@property (readonly) MPRemoteCommand *seekBackwardCommand;
@property (readonly) MPSkipIntervalCommand *skipForwardCommand;
@property (readonly) MPSkipIntervalCommand *skipBackwardCommand;
@property (readonly) MPChangePlaybackPositionCommand *changePlaybackPositionCommand;

/* Minkar: sends the command to its targets, as the media keys and the
   desktop's controls do. Returns the first status that is not success. */
- (MPRemoteCommandHandlerStatus)_sendCommand:(MPRemoteCommand *)command positionTime:(NSTimeInterval)positionTime;
@end

NS_ASSUME_NONNULL_END
