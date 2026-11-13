/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

@class NSImage;

/* The keys of a now-playing dictionary that describe the item. */
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyTitle;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyArtist;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyAlbumTitle;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyAlbumArtist;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyComposer;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyGenre;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyPlaybackDuration;
FOUNDATION_EXPORT NSString *const MPMediaItemPropertyArtwork;

@interface MPMediaItemArtwork : NSObject
- (instancetype)initWithBoundsSize:(CGSize)boundsSize requestHandler:(NSImage * (^)(CGSize size))requestHandler;
- (nullable NSImage *)imageWithSize:(CGSize)size;
@property (readonly) CGRect bounds;
@end

NS_ASSUME_NONNULL_END
