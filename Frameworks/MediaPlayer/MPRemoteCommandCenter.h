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

typedef NS_ENUM(NSInteger, MPRepeatType) {
    MPRepeatTypeOff = 0,
    MPRepeatTypeOne = 1,
    MPRepeatTypeAll = 2,
};

typedef NS_ENUM(NSInteger, MPShuffleType) {
    MPShuffleTypeOff = 0,
    MPShuffleTypeItems = 1,
    MPShuffleTypeCollections = 2,
};

@interface MPChangeRepeatModeCommand : MPRemoteCommand
@property (assign) MPRepeatType currentRepeatType;
@end

@interface MPChangeShuffleModeCommand : MPRemoteCommand
@property (assign) MPShuffleType currentShuffleType;
@end

@interface MPChangePlaybackRateCommand : MPRemoteCommand
@property (copy) NSArray<NSNumber *> *supportedPlaybackRates;
@end

@interface MPRatingCommand : MPRemoteCommand
@property (assign) float minimumRating;
@property (assign) float maximumRating;
@end

@interface MPFeedbackCommand : MPRemoteCommand
@property (assign, getter=isActive) BOOL active;
@property (copy) NSString *localizedTitle;
@property (copy) NSString *localizedShortTitle;
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
@property (readonly) MPChangeRepeatModeCommand *changeRepeatModeCommand;
@property (readonly) MPChangeShuffleModeCommand *changeShuffleModeCommand;
@property (readonly) MPChangePlaybackRateCommand *changePlaybackRateCommand;
@property (readonly) MPRatingCommand *ratingCommand;
@property (readonly) MPFeedbackCommand *likeCommand;
@property (readonly) MPFeedbackCommand *dislikeCommand;
@property (readonly) MPFeedbackCommand *bookmarkCommand;
@property (readonly) MPRemoteCommand *enableLanguageOptionCommand;
@property (readonly) MPRemoteCommand *disableLanguageOptionCommand;

/* Minkar: sends the command to its targets, as the media keys and the
   desktop's controls do. Returns the first status that is not success. */
- (MPRemoteCommandHandlerStatus)_sendCommand:(MPRemoteCommand *)command positionTime:(NSTimeInterval)positionTime;
@end

NS_ASSUME_NONNULL_END
