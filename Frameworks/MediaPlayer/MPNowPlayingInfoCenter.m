/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <MediaPlayer/MediaPlayer.h>

#define KEY(name) NSString *const name = @#name

KEY(MPMediaItemPropertyTitle);
KEY(MPMediaItemPropertyArtist);
KEY(MPMediaItemPropertyAlbumTitle);
KEY(MPMediaItemPropertyAlbumArtist);
KEY(MPMediaItemPropertyComposer);
KEY(MPMediaItemPropertyGenre);
KEY(MPMediaItemPropertyPlaybackDuration);
KEY(MPMediaItemPropertyArtwork);
KEY(MPNowPlayingInfoPropertyElapsedPlaybackTime);
KEY(MPNowPlayingInfoPropertyPlaybackRate);
KEY(MPNowPlayingInfoPropertyDefaultPlaybackRate);
KEY(MPNowPlayingInfoPropertyMediaType);
KEY(MPNowPlayingInfoPropertyIsLiveStream);
KEY(MPNowPlayingInfoPropertyAssetURL);
NSString *const MPNowPlayingInfoCenterDidChangeSessionNotification = @"org.minkar.MediaPlayer.NowPlayingDidChange";

@implementation MPMediaItemArtwork {
    NSImage * (^_handler)(CGSize);
}

- (instancetype)initWithBoundsSize:(CGSize)boundsSize requestHandler:(NSImage * (^)(CGSize))requestHandler {
    if ((self = [super init]) != nil) {
        _bounds = CGRectMake(0, 0, boundsSize.width, boundsSize.height);
        _handler = [requestHandler copy];
    }
    return self;
}

- (NSImage *)imageWithSize:(CGSize)size {
    return _handler != nil ? _handler(size) : nil;
}

@end

@implementation MPNowPlayingInfoCenter {
    NSDictionary *_info;
    MPNowPlayingPlaybackState _state;
}

+ (MPNowPlayingInfoCenter *)defaultCenter {
    static MPNowPlayingInfoCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        center = [[MPNowPlayingInfoCenter alloc] init];
    });
    return center;
}

- (NSDictionary *)nowPlayingInfo {
    @synchronized (self) {
        return _info;
    }
}

- (void)setNowPlayingInfo:(NSDictionary *)info {
    @synchronized (self) {
        _info = [info copy];
    }
    [self _post];
}

- (MPNowPlayingPlaybackState)playbackState {
    @synchronized (self) {
        return _state;
    }
}

- (void)setPlaybackState:(MPNowPlayingPlaybackState)state {
    @synchronized (self) {
        _state = state;
    }
    [self _post];
}

/* What the session hears: only strings and numbers, no artwork. */
- (void)_post {
    NSDictionary *info = self.nowPlayingInfo;
    NSMutableDictionary *summary = [NSMutableDictionary dictionary];
    summary[@"bundleIdentifier"] = [[NSBundle mainBundle] bundleIdentifier] ?: [[NSProcessInfo processInfo] processName];
    summary[@"pid"] = @([[NSProcessInfo processInfo] processIdentifier]);
    summary[@"state"] = @(self.playbackState);
    for (NSString *key in @[ MPMediaItemPropertyTitle, MPMediaItemPropertyArtist, MPMediaItemPropertyAlbumTitle,
                             MPMediaItemPropertyPlaybackDuration, MPNowPlayingInfoPropertyElapsedPlaybackTime,
                             MPNowPlayingInfoPropertyPlaybackRate ]) {
        id value = info[key];
        if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]])
            summary[key] = value;
    }
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:MPNowPlayingInfoCenterDidChangeSessionNotification
                                          object:summary[@"bundleIdentifier"] userInfo:summary
                              deliverImmediately:YES];
}

@end
