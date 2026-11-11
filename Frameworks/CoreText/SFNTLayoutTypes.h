/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Font feature types and selectors (NSFontFeatureTypeIdentifierKey and
 * NSFontFeatureSelectorIdentifierKey, kCTFontFeatureTypeIdentifierKey):
 * the numbers of the TrueType feature registry.
 */

#ifndef __SFNTLAYOUTTYPES__
#define __SFNTLAYOUTTYPES__

enum {
    kAllTypographicFeaturesType = 0,
    kLigaturesType = 1,
    kCursiveConnectionType = 2,
    kLetterCaseType = 3,
    kVerticalSubstitutionType = 4,
    kLinguisticRearrangementType = 5,
    kNumberSpacingType = 6,
    kSmartSwashType = 8,
    kDiacriticsType = 9,
    kVerticalPositionType = 10,
    kFractionsType = 11,
    kOverlappingCharactersType = 13,
    kTypographicExtrasType = 14,
    kMathematicalExtrasType = 15,
    kOrnamentSetsType = 16,
    kCharacterAlternativesType = 17,
    kDesignComplexityType = 18,
    kStyleOptionsType = 19,
    kCharacterShapeType = 20,
    kNumberCaseType = 21,
    kTextSpacingType = 22,
    kTransliterationType = 23,
    kAnnotationType = 24,
    kKanaSpacingType = 25,
    kIdeographicSpacingType = 26,
    kUnicodeDecompositionType = 27,
    kRubyKanaType = 28,
    kCJKSymbolAlternativesType = 29,
    kIdeographicAlternativesType = 30,
    kCJKVerticalRomanPlacementType = 31,
    kItalicCJKRomanType = 32,
    kCaseSensitiveLayoutType = 33,
    kAlternateKanaType = 34,
    kStylisticAlternativesType = 35,
    kContextualAlternatesType = 36,
    kLowerCaseType = 37,
    kUpperCaseType = 38,
    kLanguageTagType = 39,
    kCJKRomanSpacingType = 103,
    kLastFeatureType = -1,
};

/* kLigaturesType */
enum {
    kRequiredLigaturesOnSelector = 0, kRequiredLigaturesOffSelector = 1,
    kCommonLigaturesOnSelector = 2, kCommonLigaturesOffSelector = 3,
    kRareLigaturesOnSelector = 4, kRareLigaturesOffSelector = 5,
    kContextualLigaturesOnSelector = 18, kContextualLigaturesOffSelector = 19,
    kHistoricalLigaturesOnSelector = 20, kHistoricalLigaturesOffSelector = 21,
};

/* kNumberSpacingType */
enum {
    kMonospacedNumbersSelector = 0,
    kProportionalNumbersSelector = 1,
    kThirdWidthNumbersSelector = 2,
    kQuarterWidthNumbersSelector = 3,
};

/* kNumberCaseType */
enum {
    kLowerCaseNumbersSelector = 0,
    kUpperCaseNumbersSelector = 1,
};

/* kVerticalPositionType */
enum {
    kNormalPositionSelector = 0,
    kSuperiorsSelector = 1,
    kInferiorsSelector = 2,
    kOrdinalsSelector = 3,
    kScientificInferiorsSelector = 4,
};

/* kFractionsType */
enum {
    kNoFractionsSelector = 0,
    kVerticalFractionsSelector = 1,
    kDiagonalFractionsSelector = 2,
};

/* kTypographicExtrasType */
enum {
    kSlashedZeroOnSelector = 4,
    kSlashedZeroOffSelector = 5,
};

/* kLowerCaseType and kUpperCaseType */
enum {
    kDefaultLowerCaseSelector = 0,
    kLowerCaseSmallCapsSelector = 1,
    kLowerCasePetiteCapsSelector = 2,
    kDefaultUpperCaseSelector = 0,
    kUpperCaseSmallCapsSelector = 1,
    kUpperCasePetiteCapsSelector = 2,
};

/* kContextualAlternatesType */
enum {
    kContextualAlternatesOnSelector = 0,
    kContextualAlternatesOffSelector = 1,
};

#endif /* __SFNTLAYOUTTYPES__ */
