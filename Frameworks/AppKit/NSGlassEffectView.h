/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A glass surface behind a content view (API 25): rounded, translucent,
 * optionally tinted.
 */

#import <AppKit/NSView.h>

@class NSColor;

typedef NS_ENUM(NSInteger, NSGlassEffectViewStyle) {
    NSGlassEffectViewStyleRegular = 0,
    NSGlassEffectViewStyleClear = 1
};

API_AVAILABLE(macos(26.0))
@interface NSGlassEffectView : NSView {
    NSView *_contentView;
    CGFloat _cornerRadius;
    NSColor *_tintColor;
    NSGlassEffectViewStyle _style;
}

@property(strong) NSView *contentView;
@property CGFloat cornerRadius;
@property(copy) NSColor *tintColor;
@property NSGlassEffectViewStyle style;
@end
