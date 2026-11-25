/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSPanel.h>

/* What the font panel shows (an object answering -validModesForFontPanel:). */
typedef NSUInteger NSFontPanelModeMask;
enum {
    NSFontPanelModeMaskFace = 1 << 0,
    NSFontPanelModeMaskSize = 1 << 1,
    NSFontPanelModeMaskCollection = 1 << 2,
    NSFontPanelModeMaskUnderlineEffect = 1 << 8,
    NSFontPanelModeMaskStrikethroughEffect = 1 << 9,
    NSFontPanelModeMaskTextColorEffect = 1 << 10,
    NSFontPanelModeMaskDocumentColorEffect = 1 << 11,
    NSFontPanelModeMaskShadowEffect = 1 << 12,
    NSFontPanelModeMaskAllEffects = 0xFFF00,
    NSFontPanelModesMaskStandardModes = 0xFFFF,
    NSFontPanelModesMaskAllModes = 0xFFFFFFFF,
    NSFontPanelFaceModeMask = NSFontPanelModeMaskFace,
    NSFontPanelSizeModeMask = NSFontPanelModeMaskSize,
    NSFontPanelCollectionModeMask = NSFontPanelModeMaskCollection,
    NSFontPanelUnderlineEffectModeMask = NSFontPanelModeMaskUnderlineEffect,
    NSFontPanelStrikethroughEffectModeMask = NSFontPanelModeMaskStrikethroughEffect,
    NSFontPanelTextColorEffectModeMask = NSFontPanelModeMaskTextColorEffect,
    NSFontPanelDocumentColorEffectModeMask = NSFontPanelModeMaskDocumentColorEffect,
    NSFontPanelShadowEffectModeMask = NSFontPanelModeMaskShadowEffect,
    NSFontPanelAllEffectsModeMask = NSFontPanelModeMaskAllEffects,
    NSFontPanelStandardModesMask = NSFontPanelModesMaskStandardModes,
    NSFontPanelAllModesMask = NSFontPanelModesMaskAllModes
};

@class NSTextField, NSTableView, NSButton, NSFont, NSPopUpButton, NSColorWell, NSSlider, NSMutableArray, NSArray, NSDictionary;

/* The font panel, as the reference platform lays it out: a row of text
   effects (underline, strikethrough, text and document colors, shadow)
   above four columns: Collection, Family, Typeface and Size. A choice is
   applied at once (changeFont: and changeAttributes: down the responder
   chain). Made in code. */
@interface NSFontPanel : NSPanel {
    NSTableView *_collectionTable;
    NSTableView *_familyTable;
    NSTableView *_typefaceTable;
    NSTableView *_sizeTable;
    NSTextField *_sizeTextField;
    NSSlider *_sizeSlider;
    NSPopUpButton *_actionPopUp;
    NSPopUpButton *_underlinePopUp;
    NSPopUpButton *_strikethroughPopUp;
    NSColorWell *_textColorWell;
    NSColorWell *_documentColorWell;
    NSButton *_shadowButton;
    NSSlider *_shadowOpacitySlider;
    NSSlider *_shadowBlurSlider;
    NSView *_accessoryView;
    NSArray *_collections;
    NSArray *_families;
    NSArray *_typefaces;  /* (fontName, traitName, weight, traits) */
    NSArray *_sizes;
    NSMutableArray *_favorites;
    NSMutableArray *_recent;
    NSDictionary *_pendingAttributes; /* key -> value, NSNull to remove */
    BOOL _enabled;
    CGFloat _pointSize;
}

+ (BOOL)sharedFontPanelExists;
+ (NSFontPanel *)sharedFontPanel;

- (BOOL)isEnabled;
- (NSView *)accessoryView;

- (void)setEnabled:(BOOL)value;
- (void)setAccessoryView:(NSView *)view;

- (BOOL)worksWhenModal;
- (void)reloadDefaultFontFamilies;

- (void)setPanelFont:(NSFont *)font isMultiple:(BOOL)isMultiple;
- (NSFont *)panelConvertFont:(NSFont *)font;

- (void)set:sender;
- (void)revert:sender;

/* Minkar: as properties too, for dot syntax. */
@property (getter=isEnabled) BOOL enabled;
@end
