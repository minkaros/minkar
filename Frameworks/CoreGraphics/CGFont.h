/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreFoundation/CFData.h>
#import <CoreFoundation/CFArray.h>
#import <CoreFoundation/CFDictionary.h>
#import <CoreGraphics/CGDataProvider.h>
#include <stdint.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED

typedef struct O2Font *CGFontRef;

typedef uint16_t CGGlyph;

/* A variation axis (CGFontCopyVariationAxes' entries) and the font's
   variation (CGFontCopyVariations): the keys of their dictionaries. */
COREGRAPHICS_EXPORT const CFStringRef kCGFontVariationAxisName;
COREGRAPHICS_EXPORT const CFStringRef kCGFontVariationAxisMinValue;
COREGRAPHICS_EXPORT const CFStringRef kCGFontVariationAxisMaxValue;
COREGRAPHICS_EXPORT const CFStringRef kCGFontVariationAxisDefaultValue;

COREGRAPHICS_EXPORT CFTypeID CGFontGetTypeID(void);
COREGRAPHICS_EXPORT CGFontRef CGFontCreateWithFontName(CFStringRef name);
COREGRAPHICS_EXPORT CGFontRef CGFontRetain(CGFontRef self);
COREGRAPHICS_EXPORT void CGFontRelease(CGFontRef self);

COREGRAPHICS_EXPORT CFStringRef CGFontCopyFullName(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetUnitsPerEm(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetAscent(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetDescent(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetLeading(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetCapHeight(CGFontRef self);
COREGRAPHICS_EXPORT int CGFontGetXHeight(CGFontRef self);
COREGRAPHICS_EXPORT CGFloat CGFontGetItalicAngle(CGFontRef self);
COREGRAPHICS_EXPORT CGFloat CGFontGetStemV(CGFontRef self);
COREGRAPHICS_EXPORT CGRect CGFontGetFontBBox(CGFontRef self);

COREGRAPHICS_EXPORT size_t CGFontGetNumberOfGlyphs(CGFontRef self);
COREGRAPHICS_EXPORT bool CGFontGetGlyphAdvances(CGFontRef self, const CGGlyph *glyphs, size_t count, int *advances);

COREGRAPHICS_EXPORT CGGlyph CGFontGetGlyphWithGlyphName(CGFontRef self, CFStringRef name);
COREGRAPHICS_EXPORT CFStringRef CGFontCopyGlyphNameForGlyph(CGFontRef self, CGGlyph glyph);

COREGRAPHICS_EXPORT CFDataRef CGFontCopyTableForTag(CGFontRef self, uint32_t tag);
/* The tags of the font's tables, as the array's values (not CF objects). */
COREGRAPHICS_EXPORT CFArrayRef CGFontCopyTableTags(CGFontRef self);

COREGRAPHICS_EXPORT CGFontRef CGFontCreateWithDataProvider(CGDataProviderRef provider);
COREGRAPHICS_EXPORT CFStringRef CGFontCopyPostScriptName(CGFontRef self);
/* Boxes in font units; false when the font cannot give them. */
COREGRAPHICS_EXPORT bool CGFontGetGlyphBBoxes(CGFontRef self, const CGGlyph *glyphs, size_t count, CGRect *bboxes);

/* Variable fonts: the axes (NULL for a font without), an instance with
   some axes set (keys: the axes' names), and the values of an instance. */
COREGRAPHICS_EXPORT CFArrayRef CGFontCopyVariationAxes(CGFontRef self);
COREGRAPHICS_EXPORT CGFontRef CGFontCreateCopyWithVariations(CGFontRef self, CFDictionaryRef variations);
COREGRAPHICS_EXPORT CFDictionaryRef CGFontCopyVariations(CGFontRef self);


CF_IMPLICIT_BRIDGING_DISABLED