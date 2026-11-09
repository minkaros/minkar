/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Attribute names for attributed strings laid out by CoreText. The values
 * are AppKit's attribute names, so the same strings work with both.
 */

#import <CoreText/CoreTextExport.h>
#import <CoreFoundation/CoreFoundation.h>

CORETEXT_EXPORT const CFStringRef kCTFontAttributeName;
CORETEXT_EXPORT const CFStringRef kCTForegroundColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTForegroundColorFromContextAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBackgroundColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTParagraphStyleAttributeName;
CORETEXT_EXPORT const CFStringRef kCTUnderlineStyleAttributeName;
CORETEXT_EXPORT const CFStringRef kCTUnderlineColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTStrokeWidthAttributeName;
CORETEXT_EXPORT const CFStringRef kCTStrokeColorAttributeName;
CORETEXT_EXPORT const CFStringRef kCTKernAttributeName;
CORETEXT_EXPORT const CFStringRef kCTLigatureAttributeName;
CORETEXT_EXPORT const CFStringRef kCTSuperscriptAttributeName;
CORETEXT_EXPORT const CFStringRef kCTBaselineOffsetAttributeName;
CORETEXT_EXPORT const CFStringRef kCTVerticalFormsAttributeName;
CORETEXT_EXPORT const CFStringRef kCTGlyphInfoAttributeName;
CORETEXT_EXPORT const CFStringRef kCTLanguageAttributeName;
CORETEXT_EXPORT const CFStringRef kCTWritingDirectionAttributeName;
CORETEXT_EXPORT const CFStringRef kCTRunDelegateAttributeName;
CORETEXT_EXPORT const CFStringRef kCTTrackingAttributeName;

typedef CF_OPTIONS(int32_t, CTUnderlineStyle) {
    kCTUnderlineStyleNone = 0x00,
    kCTUnderlineStyleSingle = 0x01,
    kCTUnderlineStyleThick = 0x02,
    kCTUnderlineStyleDouble = 0x09
};

typedef CF_OPTIONS(int32_t, CTUnderlineStyleModifiers) {
    kCTUnderlinePatternSolid = 0x0000,
    kCTUnderlinePatternDot = 0x0100,
    kCTUnderlinePatternDash = 0x0200,
    kCTUnderlinePatternDashDot = 0x0300,
    kCTUnderlinePatternDashDotDot = 0x0400
};
