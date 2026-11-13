/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreText/CTFontDescriptor.h>

#define KEY(name, value) const CFStringRef name = CFSTR(value)

/* Shared with AppKit's NSFontDescriptor (NSFont*Attribute, NSFont*Trait). */
KEY(kCTFontNameAttribute, "NSFontNameAttribute");
KEY(kCTFontFamilyNameAttribute, "NSFontFamilyAttribute");
KEY(kCTFontStyleNameAttribute, "NSFontFaceAttribute");
KEY(kCTFontDisplayNameAttribute, "NSFontVisibleNameAttribute");
KEY(kCTFontTraitsAttribute, "NSFontTraitsAttribute");
KEY(kCTFontSizeAttribute, "NSFontSizeAttribute");
KEY(kCTFontMatrixAttribute, "NSFontMatrixAttribute");
KEY(kCTFontCharacterSetAttribute, "NSFontCharacterSetAttribute");
KEY(kCTFontFixedAdvanceAttribute, "NSFontFixedAdvanceAttribute");
KEY(kCTFontFeatureSettingsAttribute, "NSCTFontFeatureSettingsAttribute");
KEY(kCTFontSymbolicTrait, "NSFontSymbolicTrait");
KEY(kCTFontWeightTrait, "NSFontWeightTrait");
KEY(kCTFontWidthTrait, "NSFontWidthTrait");
KEY(kCTFontSlantTrait, "NSFontSlantTrait");
KEY(kCTFontFeatureTypeIdentifierKey, "CTFeatureTypeIdentifier");
KEY(kCTFontFeatureSelectorIdentifierKey, "CTFeatureSelectorIdentifier");

/* CoreText's own. */
KEY(kCTFontURLAttribute, "NSCTFontURLAttribute");
KEY(kCTFontVariationAttribute, "NSCTFontVariationAttribute");
KEY(kCTFontVariationAxesAttribute, "NSCTFontVariationAxesAttribute");
KEY(kCTFontLanguagesAttribute, "NSCTFontLanguagesAttribute");
KEY(kCTFontFormatAttribute, "NSCTFontFormatAttribute");
KEY(kCTFontFeaturesAttribute, "NSCTFontFeaturesAttribute");
KEY(kCTFontOpenTypeFeatureTag, "CTFeatureOpenTypeTag");
KEY(kCTFontOpenTypeFeatureValue, "CTFeatureOpenTypeValue");

KEY(kCTFontVariationAxisIdentifierKey, "NSCTVariationAxisIdentifier");
KEY(kCTFontVariationAxisMinimumValueKey, "NSCTVariationAxisMinimumValue");
KEY(kCTFontVariationAxisMaximumValueKey, "NSCTVariationAxisMaximumValue");
KEY(kCTFontVariationAxisDefaultValueKey, "NSCTVariationAxisDefaultValue");
KEY(kCTFontVariationAxisNameKey, "NSCTVariationAxisName");

KEY(kCTFontCopyrightNameKey, "CTFontCopyrightName");
KEY(kCTFontFamilyNameKey, "CTFontFamilyName");
KEY(kCTFontSubFamilyNameKey, "CTFontSubFamilyName");
KEY(kCTFontStyleNameKey, "CTFontSubFamilyName");
KEY(kCTFontUniqueNameKey, "CTFontUniqueName");
KEY(kCTFontFullNameKey, "CTFontFullName");
KEY(kCTFontVersionNameKey, "CTFontVersionName");
KEY(kCTFontPostScriptNameKey, "CTFontPostScriptName");

KEY(kCTFontManagerRegisteredFontsChangedNotification, "CTFontManagerFontChangedNotification");
