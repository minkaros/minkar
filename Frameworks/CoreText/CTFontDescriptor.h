/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Font descriptors: a font described by its attributes (name, family,
 * traits, features, variations), to find the fonts that match. The keys'
 * values are the same as AppKit's NSFontDescriptor attributes, so that a
 * descriptor's dictionary means the same to both.
 */

#ifndef __CTFONTDESCRIPTOR__
#define __CTFONTDESCRIPTOR__

#include <CoreText/CoreTextExport.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGGeometry.h>

typedef const struct __CTFontDescriptor *CTFontDescriptorRef;

/* Attributes. */
CORETEXT_EXPORT const CFStringRef kCTFontURLAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontDisplayNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFamilyNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontStyleNameAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontTraitsAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontSizeAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontMatrixAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontCharacterSetAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontLanguagesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFormatAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFeaturesAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSettingsAttribute;
CORETEXT_EXPORT const CFStringRef kCTFontFixedAdvanceAttribute;

/* kCTFontFormatAttribute's values. */
enum {
    kCTFontFormatUnrecognized = 0,
    kCTFontFormatOpenTypePostScript = 1,
    kCTFontFormatOpenTypeTrueType = 2,
    kCTFontFormatTrueType = 3,
    kCTFontFormatPostScript = 4,
    kCTFontFormatBitmap = 5
};
typedef uint32_t CTFontFormat;

/* Traits (the kCTFontTraitsAttribute dictionary). */
CORETEXT_EXPORT const CFStringRef kCTFontSymbolicTrait;
CORETEXT_EXPORT const CFStringRef kCTFontWeightTrait;
CORETEXT_EXPORT const CFStringRef kCTFontWidthTrait;
CORETEXT_EXPORT const CFStringRef kCTFontSlantTrait;

/* A feature setting (kCTFontFeatureSettingsAttribute's entries). */
CORETEXT_EXPORT const CFStringRef kCTFontFeatureTypeIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontFeatureSelectorIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontOpenTypeFeatureTag;
CORETEXT_EXPORT const CFStringRef kCTFontOpenTypeFeatureValue;

/* A variation axis (CTFontCopyVariationAxes' entries). */
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisIdentifierKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisMinimumValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisMaximumValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisDefaultValueKey;
CORETEXT_EXPORT const CFStringRef kCTFontVariationAxisNameKey;

/* The names in a font's name table (CTFontCopyName). */
CORETEXT_EXPORT const CFStringRef kCTFontCopyrightNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFamilyNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontSubFamilyNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontStyleNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontUniqueNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontFullNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontVersionNameKey;
CORETEXT_EXPORT const CFStringRef kCTFontPostScriptNameKey;

/* The font manager: posted when fonts are registered or unregistered. */
CORETEXT_EXPORT const CFStringRef kCTFontManagerRegisteredFontsChangedNotification;

/* Descriptors: an attributes dictionary, and the fonts matching it. */
CORETEXT_EXPORT CFTypeID CTFontDescriptorGetTypeID(void);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef name, CGFloat size);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithAttributes(CTFontDescriptorRef original, CFDictionaryRef attributes);
CORETEXT_EXPORT CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor);
CORETEXT_EXPORT CFTypeRef CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute);
CORETEXT_EXPORT CFTypeRef CTFontDescriptorCopyLocalizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute, CFStringRef *language);
/* Every face with the descriptor's names and symbolic traits (NULL for
   none), or the closest one. */
CORETEXT_EXPORT CFArrayRef CTFontDescriptorCreateMatchingFontDescriptors(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes);
CORETEXT_EXPORT CTFontDescriptorRef CTFontDescriptorCreateMatchingFontDescriptor(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes);

#endif
