/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreText/CoreTextExport.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGGeometry.h>

typedef const struct __CTParagraphStyle *CTParagraphStyleRef;
typedef const struct __CTTextTab *CTTextTabRef;

typedef CF_ENUM(uint8_t, CTTextAlignment) {
    kCTTextAlignmentLeft = 0,
    kCTTextAlignmentRight = 1,
    kCTTextAlignmentCenter = 2,
    kCTTextAlignmentJustified = 3,
    kCTTextAlignmentNatural = 4,
    kCTLeftTextAlignment = kCTTextAlignmentLeft,
    kCTRightTextAlignment = kCTTextAlignmentRight,
    kCTCenterTextAlignment = kCTTextAlignmentCenter,
    kCTJustifiedTextAlignment = kCTTextAlignmentJustified,
    kCTNaturalTextAlignment = kCTTextAlignmentNatural
};

typedef CF_ENUM(uint8_t, CTLineBreakMode) {
    kCTLineBreakByWordWrapping = 0,
    kCTLineBreakByCharWrapping = 1,
    kCTLineBreakByClipping = 2,
    kCTLineBreakByTruncatingHead = 3,
    kCTLineBreakByTruncatingTail = 4,
    kCTLineBreakByTruncatingMiddle = 5
};

typedef CF_ENUM(int8_t, CTWritingDirection) {
    kCTWritingDirectionNatural = -1,
    kCTWritingDirectionLeftToRight = 0,
    kCTWritingDirectionRightToLeft = 1
};

typedef CF_ENUM(uint32_t, CTParagraphStyleSpecifier) {
    kCTParagraphStyleSpecifierAlignment = 0,
    kCTParagraphStyleSpecifierFirstLineHeadIndent = 1,
    kCTParagraphStyleSpecifierHeadIndent = 2,
    kCTParagraphStyleSpecifierTailIndent = 3,
    kCTParagraphStyleSpecifierTabStops = 4,
    kCTParagraphStyleSpecifierDefaultTabInterval = 5,
    kCTParagraphStyleSpecifierLineBreakMode = 6,
    kCTParagraphStyleSpecifierLineHeightMultiple = 7,
    kCTParagraphStyleSpecifierMaximumLineHeight = 8,
    kCTParagraphStyleSpecifierMinimumLineHeight = 9,
    kCTParagraphStyleSpecifierLineSpacing = 10,
    kCTParagraphStyleSpecifierParagraphSpacing = 11,
    kCTParagraphStyleSpecifierParagraphSpacingBefore = 12,
    kCTParagraphStyleSpecifierBaseWritingDirection = 13,
    kCTParagraphStyleSpecifierMaximumLineSpacing = 14,
    kCTParagraphStyleSpecifierMinimumLineSpacing = 15,
    kCTParagraphStyleSpecifierLineSpacingAdjustment = 16,
    kCTParagraphStyleSpecifierLineBoundsOptions = 17,
    kCTParagraphStyleSpecifierCount
};

typedef struct CTParagraphStyleSetting {
    CTParagraphStyleSpecifier spec;
    size_t valueSize;
    const void *value;
} CTParagraphStyleSetting;

CORETEXT_EXPORT CFTypeID CTParagraphStyleGetTypeID(void);
CORETEXT_EXPORT CTParagraphStyleRef CTParagraphStyleCreate(const CTParagraphStyleSetting *settings, size_t settingCount);
CORETEXT_EXPORT CTParagraphStyleRef CTParagraphStyleCreateCopy(CTParagraphStyleRef paragraphStyle);
CORETEXT_EXPORT bool CTParagraphStyleGetValueForSpecifier(CTParagraphStyleRef paragraphStyle, CTParagraphStyleSpecifier spec,
                                                          size_t valueBufferSize, void *valueBuffer);

CORETEXT_EXPORT const CFStringRef kCTTabColumnTerminatorsAttributeName;

CORETEXT_EXPORT CFTypeID CTTextTabGetTypeID(void);
CORETEXT_EXPORT CTTextTabRef CTTextTabCreate(CTTextAlignment alignment, double location, CFDictionaryRef options);
CORETEXT_EXPORT CTTextAlignment CTTextTabGetAlignment(CTTextTabRef tab);
CORETEXT_EXPORT double CTTextTabGetLocation(CTTextTabRef tab);
CORETEXT_EXPORT CFDictionaryRef CTTextTabGetOptions(CTTextTabRef tab);
