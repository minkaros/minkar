/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A line of laid-out text: runs of glyphs, one per run of attributes.
 */

#import <CoreText/CoreTextExport.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>

typedef const struct __CTLine *CTLineRef;
typedef const struct __CTRun *CTRunRef;

typedef CF_OPTIONS(CFOptionFlags, CTLineBoundsOptions) {
    kCTLineBoundsExcludeTypographicLeading = 1 << 0,
    kCTLineBoundsExcludeTypographicShifts = 1 << 1,
    kCTLineBoundsUseHangingPunctuation = 1 << 2,
    kCTLineBoundsUseGlyphPathBounds = 1 << 3,
    kCTLineBoundsUseOpticalBounds = 1 << 4,
    kCTLineBoundsIncludeLanguageExtents = 1 << 5
};

typedef CF_ENUM(uint32_t, CTLineTruncationType) {
    kCTLineTruncationStart = 0,
    kCTLineTruncationEnd = 1,
    kCTLineTruncationMiddle = 2
};

typedef CF_OPTIONS(uint32_t, CTRunStatus) {
    kCTRunStatusNoStatus = 0,
    kCTRunStatusRightToLeft = (1 << 0),
    kCTRunStatusNonMonotonic = (1 << 1),
    kCTRunStatusHasNonIdentityMatrix = (1 << 2)
};

CORETEXT_EXPORT CFTypeID CTLineGetTypeID(void);
CORETEXT_EXPORT CTLineRef CTLineCreateWithAttributedString(CFAttributedStringRef attrString);
CORETEXT_EXPORT CTLineRef CTLineCreateTruncatedLine(CTLineRef line, double width, CTLineTruncationType truncationType, CTLineRef truncationToken);
CORETEXT_EXPORT CFIndex CTLineGetGlyphCount(CTLineRef line);
CORETEXT_EXPORT CFArrayRef CTLineGetGlyphRuns(CTLineRef line);
CORETEXT_EXPORT CFRange CTLineGetStringRange(CTLineRef line);
CORETEXT_EXPORT double CTLineGetPenOffsetForFlush(CTLineRef line, CGFloat flushFactor, double flushWidth);
CORETEXT_EXPORT void CTLineDraw(CTLineRef line, CGContextRef context);
CORETEXT_EXPORT CGRect CTLineGetBoundsWithOptions(CTLineRef line, CTLineBoundsOptions options);
CORETEXT_EXPORT double CTLineGetTypographicBounds(CTLineRef line, CGFloat *ascent, CGFloat *descent, CGFloat *leading);
CORETEXT_EXPORT CGRect CTLineGetImageBounds(CTLineRef line, CGContextRef context);
CORETEXT_EXPORT double CTLineGetTrailingWhitespaceWidth(CTLineRef line);
CORETEXT_EXPORT CFIndex CTLineGetStringIndexForPosition(CTLineRef line, CGPoint position);
CORETEXT_EXPORT CGFloat CTLineGetOffsetForStringIndex(CTLineRef line, CFIndex charIndex, CGFloat *secondaryOffset);

CORETEXT_EXPORT CFTypeID CTRunGetTypeID(void);
CORETEXT_EXPORT CFIndex CTRunGetGlyphCount(CTRunRef run);
CORETEXT_EXPORT CFDictionaryRef CTRunGetAttributes(CTRunRef run);
CORETEXT_EXPORT CTRunStatus CTRunGetStatus(CTRunRef run);
CORETEXT_EXPORT const CGGlyph *CTRunGetGlyphsPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetGlyphs(CTRunRef run, CFRange range, CGGlyph *buffer);
CORETEXT_EXPORT const CGPoint *CTRunGetPositionsPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetPositions(CTRunRef run, CFRange range, CGPoint *buffer);
CORETEXT_EXPORT const CGSize *CTRunGetAdvancesPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetAdvances(CTRunRef run, CFRange range, CGSize *buffer);
CORETEXT_EXPORT const CFIndex *CTRunGetStringIndicesPtr(CTRunRef run);
CORETEXT_EXPORT void CTRunGetStringIndices(CTRunRef run, CFRange range, CFIndex *buffer);
CORETEXT_EXPORT CFRange CTRunGetStringRange(CTRunRef run);
CORETEXT_EXPORT double CTRunGetTypographicBounds(CTRunRef run, CFRange range, CGFloat *ascent, CGFloat *descent, CGFloat *leading);
CORETEXT_EXPORT void CTRunDraw(CTRunRef run, CGContextRef context, CFRange range);
