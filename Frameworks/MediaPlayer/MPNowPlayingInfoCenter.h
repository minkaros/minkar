/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <MediaPlayer/MPMediaItem.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MPNowPlayingPlaybackState) {
    MPNowPlayingPlaybackStateUnknown = 0,
    MPNowPlayingPlaybackStatePlaying,
    MPNowPlayingPlaybackStatePaused,
    MPNowPlayingPlaybackStateStopped,
    MPNowPlayingPlaybackStateInterrupted,
};

typedef NS_ENUM(NSUInteger, MPNowPlayingInfoMediaType) {
    MPNowPlayingInfoMediaTypeNone = 0,
    MPNowPlayingInfoMediaTypeAudio,
    MPNowPlayingInfoMediaTypeVideo,
};

/* The keys of a now-playing dictionary that describe the playback. */
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyElapsedPlaybackTime;
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyPlaybackRate;
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyDefaultPlaybackRate;
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyMediaType;
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyIsLiveStream;
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoPropertyAssetURL;

/* Posted in the session when an app's now-playing information changes;
   the user info has its bundle identifier, title, artist and state. */
FOUNDATION_EXPORT NSString *const MPNowPlayingInfoCenterDidChangeSessionNotification;

@interface MPNowPlayingInfoCenter : NSObject
+ (MPNowPlayingInfoCenter *)defaultCenter;
@property (copy, nullable) NSDictionary<NSString *, id> *nowPlayingInfo;
@property MPNowPlayingPlaybackState playbackState;
@end

NS_ASSUME_NONNULL_END
