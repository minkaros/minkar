// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSTouchBar.h>

@class NSColor, NSColorList, NSColorSpace, NSImage;

NS_ASSUME_NONNULL_BEGIN

/* A Touch Bar item that picks a color: its color, the colors it offers
   and what it sends when one is picked. */
@interface NSColorPickerTouchBarItem : NSTouchBarItem
+ (instancetype)colorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier;
+ (instancetype)textColorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier;
+ (instancetype)strokeColorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier;
+ (instancetype)colorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier buttonImage:(NSImage *)image;
@property (copy) NSColor *color;
@property BOOL showsAlpha;
@property (nullable, copy) NSArray<NSColorSpace *> *allowedColorSpaces;
@property (null_resettable, strong) NSColorList *colorList;
@property (readwrite, copy, null_resettable) NSString *customizationLabel;
@property (nullable, assign) id target;
@property (nullable) SEL action;
@property (getter=isEnabled) BOOL enabled;
@end

NS_ASSUME_NONNULL_END
