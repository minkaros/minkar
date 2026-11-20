// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSView.h>
#import <AppKit/NSImageCell.h>

@class NSScrubberLayoutAttributes, NSTextField, NSImageView, NSImage;

NS_ASSUME_NONNULL_BEGIN

/* A view a scrubber arranges: an item, or the selection drawn over or
   under one. */
@interface NSScrubberArrangedView : NSView
@property (getter=isSelected) BOOL selected;
@property (getter=isHighlighted) BOOL highlighted;
- (void)applyLayoutAttributes:(NSScrubberLayoutAttributes *)layoutAttributes;
@end

@interface NSScrubberSelectionView : NSScrubberArrangedView
@end

@interface NSScrubberItemView : NSScrubberArrangedView
@end

@interface NSScrubberTextItemView : NSScrubberItemView
@property (readonly, strong) NSTextField *textField;
@property (copy) NSString *title;
@end

@interface NSScrubberImageItemView : NSScrubberItemView
@property (readonly, strong) NSImageView *imageView;
@property (copy) NSImage *image;
@property NSImageAlignment imageAlignment;
@end

NS_ASSUME_NONNULL_END
