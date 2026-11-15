/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A view that shows a player's video, with its playback controls.
 */

#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVPlayerViewControlsStyle) {
    AVPlayerViewControlsStyleNone,
    AVPlayerViewControlsStyleInline,
    AVPlayerViewControlsStyleFloating,
    AVPlayerViewControlsStyleMinimal,
    AVPlayerViewControlsStyleDefault = AVPlayerViewControlsStyleInline,
};

typedef NS_ENUM(NSInteger, AVPlayerViewTrimResult) {
    AVPlayerViewTrimOKButton,
    AVPlayerViewTrimCancelButton,
};

@interface AVPlayerView : NSView
@property (nullable) AVPlayer *player;
@property AVPlayerViewControlsStyle controlsStyle;
@property (copy) AVLayerVideoGravity videoGravity;
@property (readonly, getter=isReadyForDisplay) BOOL readyForDisplay;
@property (readonly) NSRect videoBounds;
@property (readonly, nullable) NSView *contentOverlayView;
@property BOOL updatesNowPlayingInfoCenter;
@property (nullable) NSMenu *actionPopUpButtonMenu;
@property BOOL showsFullScreenToggleButton;
@property BOOL showsTimecodes;
@property BOOL showsFrameSteppingButtons;
@property BOOL showsSharingServiceButton;
@property BOOL allowsPictureInPicturePlayback;
@property (readonly) BOOL canBeginTrimming;
- (void)beginTrimmingWithCompletionHandler:(void (^_Nullable)(AVPlayerViewTrimResult result))handler;
- (void)flashChapterNumber:(NSUInteger)chapterNumber chapterTitle:(NSString *)chapterTitle;
@end

NS_ASSUME_NONNULL_END
