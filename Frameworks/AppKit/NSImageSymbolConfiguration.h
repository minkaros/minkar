/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * How a symbol image is drawn (API 21): its point size and weight, its
 * scale beside text, and its colors.
 */

#import <Foundation/Foundation.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSFont.h>

@class NSColor;

typedef NS_ENUM(NSInteger, NSImageSymbolScale) {
    NSImageSymbolScaleSmall = 1,
    NSImageSymbolScaleMedium = 2,
    NSImageSymbolScaleLarge = 3
};

API_AVAILABLE(macos(11.0))
@interface NSImageSymbolConfiguration : NSObject <NSCopying, NSSecureCoding> {
    CGFloat _pointSize;
    NSFontWeight _weight;
    NSImageSymbolScale _scale;
    NSString *_textStyle;
    NSColor *_hierarchicalColor;
    NSArray *_paletteColors;
    BOOL _prefersMulticolor;
}

+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight scale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight;
+ (instancetype)configurationWithTextStyle:(NSString *)style scale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithTextStyle:(NSString *)style;
+ (instancetype)configurationWithScale:(NSImageSymbolScale)scale;
+ (instancetype)configurationWithHierarchicalColor:(NSColor *)hierarchicalColor;
+ (instancetype)configurationWithPaletteColors:(NSArray<NSColor *> *)paletteColors;
+ (instancetype)configurationPreferringMulticolor;
- (instancetype)configurationByApplyingConfiguration:(NSImageSymbolConfiguration *)configuration;
@end

@interface NSImage (SymbolConfiguration)
- (NSImage *)imageWithSymbolConfiguration:(NSImageSymbolConfiguration *)configuration API_AVAILABLE(macos(12.0));
@property(readonly, copy) NSImageSymbolConfiguration *symbolConfiguration API_AVAILABLE(macos(12.0));
- (NSImage *)imageWithTintColor:(NSColor *)color API_AVAILABLE(macos(12.0));
@end
