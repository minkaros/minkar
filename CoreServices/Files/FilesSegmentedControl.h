/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The window bar's segmented controls (Atlas): a light capsule of
 * segments, each a glyph drawn in code; the back and forward pair, which
 * acts at once, and the view modes, one of which is selected.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, FilesGlyph) {
    FilesGlyphBack,
    FilesGlyphForward,
    FilesGlyphIcons,
    FilesGlyphList,
    FilesGlyphColumns,
};

@interface FilesSegmentedControl : NSView
/* Glyphs, one per segment (NSNumbers of FilesGlyph), and an action for
   each, sent to target with the control. */
- (instancetype)initWithGlyphs:(NSArray<NSNumber *> *)glyphs actions:(NSArray<NSString *> *)actions
                        target:(id)target;
/* The selected segment (view modes), or -1 (none: a momentary pair). */
@property (nonatomic) NSInteger selectedSegment;
- (void)setEnabled:(BOOL)enabled forSegment:(NSInteger)segment;
- (BOOL)isEnabledForSegment:(NSInteger)segment;
/* What each segment does, for tooltips and accessibility. */
- (void)setLabel:(NSString *)label forSegment:(NSInteger)segment;
/* Its natural size: 28 points a segment, 24 high. */
- (NSSize)naturalSize;
@end

NS_ASSUME_NONNULL_END
