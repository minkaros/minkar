/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The annotation keys and values, the reference's (measured 19 Nov 2026).
 */

#import <PDFKit/PDFKitPlatform.h>

NS_ASSUME_NONNULL_BEGIN

typedef NSString *PDFAnnotationKey NS_STRING_ENUM;
typedef NSString *PDFAnnotationSubtype NS_STRING_ENUM;
typedef NSString *PDFAnnotationWidgetSubtype NS_STRING_ENUM;
typedef NSString *PDFAnnotationLineEndingStyle NS_STRING_ENUM;
typedef NSString *PDFAnnotationHighlightingMode NS_STRING_ENUM;
typedef NSString *PDFAppearanceCharacteristicsKey NS_STRING_ENUM;

typedef NS_ENUM(NSInteger, PDFLineStyle) {
    kPDFLineStyleNone = 0,
    kPDFLineStyleSquare = 1,
    kPDFLineStyleCircle = 2,
    kPDFLineStyleDiamond = 3,
    kPDFLineStyleOpenArrow = 4,
    kPDFLineStyleClosedArrow = 5
};

typedef NS_ENUM(NSInteger, PDFTextAnnotationIconType) {
    kPDFTextAnnotationIconComment = 0,
    kPDFTextAnnotationIconKey = 1,
    kPDFTextAnnotationIconNote = 2,
    kPDFTextAnnotationIconHelp = 3,
    kPDFTextAnnotationIconNewParagraph = 4,
    kPDFTextAnnotationIconParagraph = 5,
    kPDFTextAnnotationIconInsert = 6
};

typedef NS_ENUM(NSInteger, PDFMarkupType) {
    kPDFMarkupTypeHighlight = 0,
    kPDFMarkupTypeStrikeOut = 1,
    kPDFMarkupTypeUnderline = 2,
    kPDFMarkupTypeRedact = 3
};

typedef NS_ENUM(NSInteger, PDFWidgetControlType) {
    kPDFWidgetUnknownControl = -1,
    kPDFWidgetPushButtonControl = 0,
    kPDFWidgetRadioButtonControl = 1,
    kPDFWidgetCheckBoxControl = 2
};

typedef NS_ENUM(NSInteger, PDFWidgetCellState) {
    kPDFWidgetMixedState = -1,
    kPDFWidgetOffState = 0,
    kPDFWidgetOnState = 1
};

PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyAppearanceDictionary;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyAppearanceState;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyBorder;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyColor;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyContents;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyFlags;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyDate;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyName;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyPage;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyPopup;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyRect;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeySubtype;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyAction;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyAdditionalActions;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyBorderStyle;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyDefaultAppearance;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyDestination;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyHighlightingMode;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyInklist;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyInteriorColor;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyLinePoints;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyLineEndingStyles;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyIconName;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyOpen;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyParent;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyQuadding;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyQuadPoints;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyTextLabel;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetDownCaption;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetBorderColor;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetBackgroundColor;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetCaption;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetDefaultValue;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetFieldFlags;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetFieldType;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetAppearanceDictionary;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetMaxLen;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetOptions;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetRotation;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetRolloverCaption;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetTextLabelUI;
PDFKIT_EXTERN PDFAnnotationKey const PDFAnnotationKeyWidgetValue;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeText;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeLink;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeFreeText;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeLine;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeSquare;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeCircle;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeHighlight;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeUnderline;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeStrikeOut;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeInk;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeStamp;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypePopup;
PDFKIT_EXTERN PDFAnnotationSubtype const PDFAnnotationSubtypeWidget;
PDFKIT_EXTERN PDFAnnotationWidgetSubtype const PDFAnnotationWidgetSubtypeButton;
PDFKIT_EXTERN PDFAnnotationWidgetSubtype const PDFAnnotationWidgetSubtypeChoice;
PDFKIT_EXTERN PDFAnnotationWidgetSubtype const PDFAnnotationWidgetSubtypeSignature;
PDFKIT_EXTERN PDFAnnotationWidgetSubtype const PDFAnnotationWidgetSubtypeText;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleNone;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleSquare;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleCircle;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleDiamond;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleOpenArrow;
PDFKIT_EXTERN PDFAnnotationLineEndingStyle const PDFAnnotationLineEndingStyleClosedArrow;
PDFKIT_EXTERN PDFAnnotationHighlightingMode const PDFAnnotationHighlightingModeNone;
PDFKIT_EXTERN PDFAnnotationHighlightingMode const PDFAnnotationHighlightingModeInvert;
PDFKIT_EXTERN PDFAnnotationHighlightingMode const PDFAnnotationHighlightingModeOutline;
PDFKIT_EXTERN PDFAnnotationHighlightingMode const PDFAnnotationHighlightingModePush;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyBackgroundColor;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyBorderColor;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyRotation;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyCaption;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyRolloverCaption;
PDFKIT_EXTERN PDFAppearanceCharacteristicsKey const PDFAppearanceCharacteristicsKeyDownCaption;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeComment;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeKey;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeNote;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeHelp;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeNewParagraph;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeParagraph;
PDFKIT_EXTERN NSString *const PDFAnnotationTextIconTypeInsert;

NS_ASSUME_NONNULL_END
