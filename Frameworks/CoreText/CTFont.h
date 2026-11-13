/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/* Minkar: C can include it too (ApplicationServices.h does): the same
   types as opaque pointers and CF types there. */
#ifdef __OBJC__
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif
#import <CoreText/CoreTextExport.h>
#import <CoreGraphics/CGAffineTransform.h>
#import <CoreGraphics/CGFont.h>
#import <CoreGraphics/CGPath.h>
#import <CoreGraphics/CGContext.h>
#import <CoreText/CTFontDescriptor.h>

/* A CF type in the API; CoreText's KTFont objects behind it. */
typedef const struct __CTFont *CTFontRef;
#define CT_FONT_ATTRIBUTES CFTypeRef
#define CT_FONT_LANGUAGE CFStringRef

/* Minkar: the UI font names of API 10.5 (kCTFontUIFont*) beside the older
   kCT...FontType ones; the same values. */
typedef enum {
    kCTFontUIFontNone = (uint32_t)-1,
    kCTFontUIFontUser = 0,
    kCTFontUIFontUserFixedPitch = 1,
    kCTFontUIFontSystem = 2,
    kCTFontUIFontEmphasizedSystem = 3,
    kCTFontUIFontSmallSystem = 4,
    kCTFontUIFontSmallEmphasizedSystem = 5,
    kCTFontUIFontMiniSystem = 6,
    kCTFontUIFontMiniEmphasizedSystem = 7,
    kCTFontUIFontViews = 8,
    kCTFontUIFontApplication = 9,
    kCTFontUIFontLabel = 10,
    kCTFontUIFontMenuTitle = 11,
    kCTFontUIFontMenuItem = 12,
    kCTFontUIFontMenuItemMark = 13,
    kCTFontUIFontMenuItemCmdKey = 14,
    kCTFontUIFontWindowTitle = 15,
    kCTFontUIFontPushButton = 16,
    kCTFontUIFontUtilityWindowTitle = 17,
    kCTFontUIFontAlertHeader = 18,
    kCTFontUIFontSystemDetail = 19,
    kCTFontUIFontEmphasizedSystemDetail = 20,
    kCTFontUIFontToolbar = 21,
    kCTFontUIFontSmallToolbar = 22,
    kCTFontUIFontMessage = 23,
    kCTFontUIFontPalette = 24,
    kCTFontUIFontToolTip = 25,
    kCTFontUIFontControlContent = 26,
    kCTFontNoFontType = (uint32_t)-1,
    kCTFontUserFontType = 0,
    kCTFontUserFixedPitchFontType = 1,
    kCTFontSystemFontType = 2,
    kCTFontEmphasizedSystemFontType = 3,
    kCTFontSmallSystemFontType = 4,
    kCTFontSmallEmphasizedSystemFontType = 5,
    kCTFontMiniSystemFontType = 6,
    kCTFontMiniEmphasizedSystemFontType = 7,
    kCTFontViewsFontType = 8,
    kCTFontApplicationFontType = 9,
    kCTFontLabelFontType = 10,
    kCTFontMenuTitleFontType = 11,
    kCTFontMenuItemFontType = 12,
    kCTFontMenuItemMarkFontType = 13,
    kCTFontMenuItemCmdKeyFontType = 14,
    kCTFontWindowTitleFontType = 15,
    kCTFontPushButtonFontType = 16,
    kCTFontUtilityWindowTitleFontType = 17,
    kCTFontAlertHeaderFontType = 18,
    kCTFontSystemDetailFontType = 19,
    kCTFontEmphasizedSystemDetailFontType = 20,
    kCTFontToolbarFontType = 21,
    kCTFontSmallToolbarFontType = 22,
    kCTFontMessageFontType = 23,
    kCTFontPaletteFontType = 24,
    kCTFontToolTipFontType = 25,
    kCTFontControlContentFontType = 26
} CTFontUIFontType;

/* How glyphs are laid out (API 10.5; the older names before 10.8). */
typedef enum {
    kCTFontOrientationDefault = 0,
    kCTFontOrientationHorizontal = 1,
    kCTFontOrientationVertical = 2,
    kCTFontDefaultOrientation = kCTFontOrientationDefault,
    kCTFontHorizontalOrientation = kCTFontOrientationHorizontal,
    kCTFontVerticalOrientation = kCTFontOrientationVertical,
} CTFontOrientation;

CORETEXT_EXPORT CTFontRef CTFontCreateWithGraphicsFont(CGFontRef cgFont, CGFloat size, CGAffineTransform *xform, CT_FONT_ATTRIBUTES attributes);
CORETEXT_EXPORT CTFontRef CTFontCreateUIFontForLanguage(CTFontUIFontType uiFontType, CGFloat size, CT_FONT_LANGUAGE language);
CORETEXT_EXPORT CFStringRef CTFontCopyFullName(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetSize(CTFontRef self);
CORETEXT_EXPORT size_t CTFontGetGlyphCount(CTFontRef self);
CORETEXT_EXPORT bool CTFontGetGlyphsForCharacters(CTFontRef self, const UniChar *characters, CGGlyph *glyphs, size_t count);
CORETEXT_EXPORT CGRect CTFontGetBoundingBox(CTFontRef self);
/* The glyphs' advances (into advances, which may be NULL) and their sum. */
CORETEXT_EXPORT double CTFontGetAdvancesForGlyphs(CTFontRef self, CTFontOrientation orientation, const CGGlyph *glyphs, CGSize *advances, size_t count);
CORETEXT_EXPORT CGFloat CTFontGetUnderlinePosition(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetUnderlineThickness(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetAscent(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetDescent(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetLeading(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetSlantAngle(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetXHeight(CTFontRef self);
CORETEXT_EXPORT CGFloat CTFontGetCapHeight(CTFontRef self);
CORETEXT_EXPORT CGPathRef CTFontCreatePathForGlyph(CTFontRef self, CGGlyph glyph, CGAffineTransform *xform);

typedef CF_OPTIONS(uint32_t, CTFontSymbolicTraits) {
    kCTFontTraitItalic = (1 << 0),
    kCTFontTraitBold = (1 << 1),
    kCTFontTraitExpanded = (1 << 5),
    kCTFontTraitCondensed = (1 << 6),
    kCTFontTraitMonoSpace = (1 << 10),
    kCTFontTraitVertical = (1 << 11),
    kCTFontTraitUIOptimized = (1 << 12),
    kCTFontTraitColorGlyphs = (1 << 13),
    kCTFontTraitComposite = (1 << 14),
    kCTFontTraitClassMask = (15U << 28),
    kCTFontItalicTrait = kCTFontTraitItalic,
    kCTFontBoldTrait = kCTFontTraitBold,
    kCTFontExpandedTrait = kCTFontTraitExpanded,
    kCTFontCondensedTrait = kCTFontTraitCondensed,
    kCTFontMonoSpaceTrait = kCTFontTraitMonoSpace,
    kCTFontVerticalTrait = kCTFontTraitVertical,
    kCTFontUIOptimizedTrait = kCTFontTraitUIOptimized,
    kCTFontColorGlyphsTrait = kCTFontTraitColorGlyphs,
    kCTFontCompositeTrait = kCTFontTraitComposite,
    kCTFontClassMaskTrait = kCTFontTraitClassMask
};

/* A font by PostScript or full name; size 0 is 12 points. */
CORETEXT_EXPORT CTFontRef CTFontCreateWithName(CFStringRef name, CGFloat size, const CGAffineTransform *matrix);
CORETEXT_EXPORT CTFontRef CTFontCreateCopyWithAttributes(CTFontRef font, CGFloat size, const CGAffineTransform *matrix, CT_FONT_ATTRIBUTES attributes);
/* The same family with the traits changed (bold and italic); NULL when
   the family has no such face. */
CORETEXT_EXPORT CTFontRef CTFontCreateCopyWithSymbolicTraits(CTFontRef font, CGFloat size, const CGAffineTransform *matrix,
                                                             CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask);
CORETEXT_EXPORT CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyPostScriptName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyFamilyName(CTFontRef font);
CORETEXT_EXPORT CFStringRef CTFontCopyDisplayName(CTFontRef font);
CORETEXT_EXPORT CGFontRef CTFontCopyGraphicsFont(CTFontRef font, CT_FONT_ATTRIBUTES *attributes);
CORETEXT_EXPORT CGFloat CTFontGetUnitsPerEm(CTFontRef font);
CORETEXT_EXPORT CFTypeID CTFontGetTypeID(void);

/* Fonts from descriptors, and what describes a font. */
CORETEXT_EXPORT CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef descriptor, CGFloat size, const CGAffineTransform *matrix);
CORETEXT_EXPORT CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font);
CORETEXT_EXPORT CFTypeRef CTFontCopyAttribute(CTFontRef font, CFStringRef attribute);
CORETEXT_EXPORT CFStringRef CTFontCopyName(CTFontRef font, CFStringRef nameKey);
CORETEXT_EXPORT CFStringRef CTFontCopyLocalizedName(CTFontRef font, CFStringRef nameKey, CFStringRef *actualLanguage);
CORETEXT_EXPORT CFCharacterSetRef CTFontCopyCharacterSet(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopyFeatures(CTFontRef font);
CORETEXT_EXPORT CFArrayRef CTFontCopyVariationAxes(CTFontRef font);
CORETEXT_EXPORT CFDictionaryRef CTFontCopyVariation(CTFontRef font);

/* The font's sfnt tables; the available ones' tags are the array's values. */
typedef FourCharCode CTFontTableTag;
typedef uint32_t CTFontTableOptions;
enum {
    kCTFontTableOptionNoOptions = 0,
    kCTFontTableOptionExcludeSynthetic = (1 << 0)
};
CORETEXT_EXPORT CFArrayRef CTFontCopyAvailableTables(CTFontRef font, CTFontTableOptions options);
CORETEXT_EXPORT CFDataRef CTFontCopyTable(CTFontRef font, CTFontTableTag table, CTFontTableOptions options);

/* A font that can draw the string's characters (the font itself when it
   can), drawing glyphs, and their boxes. */
CORETEXT_EXPORT CTFontRef CTFontCreateForString(CTFontRef currentFont, CFStringRef string, CFRange range);
CORETEXT_EXPORT void CTFontDrawGlyphs(CTFontRef font, const CGGlyph glyphs[], const CGPoint positions[], size_t count, CGContextRef context);
CORETEXT_EXPORT CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef font, CTFontOrientation orientation, const CGGlyph glyphs[], CGRect *boundingRects, CFIndex count);
