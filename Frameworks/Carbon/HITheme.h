/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Appearance Manager and HITheme, as HIToolbox documents them: theme
 * metrics, brushes and text colours, and the drawing of controls
 * (buttons, tracks, tabs, frames, group boxes, separators, the grow box,
 * backgrounds) into a CGContext. Minkar draws them in Atlas's look.
 */

#ifndef __HITHEME__
#define __HITHEME__

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CTFont.h>
#include <Carbon/HIShape.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __MINKAR_RGBCOLOR__
#define __MINKAR_RGBCOLOR__
typedef struct RGBColor {
    unsigned short red;
    unsigned short green;
    unsigned short blue;
} RGBColor;
#endif

enum { themeInvalidMetricErr = -30570, themeBadTextColorErr = -30569, themeBadCursorIndexErr = -30568 };

/* ---- Metrics, in points (Atlas's) ---- */

typedef UInt32 ThemeMetric;
enum {
    kThemeMetricScrollBarWidth = 0,
    kThemeMetricSmallScrollBarWidth = 1,
    kThemeMetricCheckBoxHeight = 2,
    kThemeMetricRadioButtonHeight = 3,
    kThemeMetricEditTextWhitespace = 4,
    kThemeMetricEditTextFrameOutset = 5,
    kThemeMetricListBoxFrameOutset = 6,
    kThemeMetricFocusRectOutset = 7,
    kThemeMetricImageWellThickness = 8,
    kThemeMetricScrollBarOverlap = 9,
    kThemeMetricLargeTabHeight = 10,
    kThemeMetricLargeTabCapsWidth = 11,
    kThemeMetricTabFrameOverlap = 12,
    kThemeMetricTabIndentOrStyle = 13,
    kThemeMetricTabOverlap = 14,
    kThemeMetricSmallTabHeight = 15,
    kThemeMetricSmallTabCapsWidth = 16,
    kThemeMetricPushButtonHeight = 17,
    kThemeMetricListHeaderHeight = 18,
    kThemeMetricDisclosureTriangleHeight = 19,
    kThemeMetricDisclosureTriangleWidth = 20,
    kThemeMetricLittleArrowsHeight = 21,
    kThemeMetricLittleArrowsWidth = 22,
    kThemeMetricPopupButtonHeight = 23,
    kThemeMetricSmallPopupButtonHeight = 24,
    kThemeMetricLargeProgressBarThickness = 25,
    kThemeMetricPullDownHeight = 26,
    kThemeMetricSmallPullDownHeight = 27,
    kThemeMetricResizeControlHeight = 28,
    kThemeMetricSmallResizeControlHeight = 29,
    kThemeMetricHSliderHeight = 30,
    kThemeMetricVSliderWidth = 31,
    kThemeMetricCheckBoxWidth = 32,
    kThemeMetricRadioButtonWidth = 33,
    kThemeMetricNormalProgressBarThickness = 34,
    kThemeMetricProgressBarShadowOutset = 35,
    kThemeMetricMenuMarkColumnWidth = 36,
    kThemeMetricMenuExcludedMarkColumnWidth = 37,
    kThemeMetricMenuMarkIndent = 38,
    kThemeMetricMenuTextLeadingEdgeMargin = 39,
    kThemeMetricMenuTextTrailingEdgeMargin = 40,
    kThemeMetricMenuIndentWidth = 41,
    kThemeMetricMenuIconTrailingEdgeMargin = 42,
    kThemeMetricSmallCheckBoxHeight = 43,
    kThemeMetricSmallCheckBoxWidth = 44,
    kThemeMetricSmallRadioButtonHeight = 45,
    kThemeMetricSmallRadioButtonWidth = 46,
    kThemeMetricCount
};
OSStatus GetThemeMetric(ThemeMetric inMetric, SInt32 *outMetric);

typedef UInt16 ThemeMenuItemType;
enum { kThemeMenuItemPlain = 0, kThemeMenuItemHierarchical = 1, kThemeMenuItemScrollUpArrow = 2,
       kThemeMenuItemScrollDownArrow = 3, kThemeMenuItemAtTop = 0x0100, kThemeMenuItemAtBottom = 0x0200,
       kThemeMenuItemHierBackground = 0x0400, kThemeMenuItemPopUpBackground = 0x0800,
       kThemeMenuItemHasIcon = 0x8000, kThemeMenuItemNoBackground = 0x4000 };
OSStatus GetThemeMenuSeparatorHeight(SInt16 *outHeight);
OSStatus GetThemeMenuItemExtra(ThemeMenuItemType inItemType, SInt16 *outHeight, SInt16 *outWidth);

/* ---- Fonts ---- */

typedef UInt16 ThemeFontID;
enum {
    kThemeSystemFont = 0, kThemeSmallSystemFont = 1, kThemeSmallEmphasizedSystemFont = 2,
    kThemeViewsFont = 3, kThemeEmphasizedSystemFont = 4, kThemeApplicationFont = 5,
    kThemeLabelFont = 6, kThemeMenuTitleFont = 100, kThemeMenuItemFont = 101,
    kThemeMenuItemMarkFont = 102, kThemeMenuItemCmdKeyFont = 103, kThemeWindowTitleFont = 104,
    kThemePushButtonFont = 105, kThemeUtilityWindowTitleFont = 106, kThemeAlertHeaderFont = 107,
    kThemeSystemFontDetail = 7, kThemeSystemFontDetailEmphasized = 8, kThemeToolbarFont = 108,
    kThemeMiniSystemFont = 109, kThemeCurrentPortFont = 200
};
CTFontUIFontType HIThemeGetUIFontType(ThemeFontID inFontID);

/* ---- Drawing states ---- */

typedef UInt32 ThemeDrawState;
enum {
    kThemeStateInactive = 0,
    kThemeStateActive = 1,
    kThemeStatePressed = 2,
    kThemeStateRollover = 6,
    kThemeStateUnavailable = 7,
    kThemeStateUnavailableInactive = 8,
    kThemeStateDisabled = 0,
    kThemeStatePressedUp = 2,
    kThemeStatePressedDown = 3
};

typedef UInt32 HIThemeOrientation;
enum { kHIThemeOrientationNormal = 0, kHIThemeOrientationInverted = 1 };

/* ---- Brushes and text colours ---- */

typedef SInt16 ThemeBrush;
enum {
    kThemeBrushDialogBackgroundActive = 1, kThemeBrushDialogBackgroundInactive = 2,
    kThemeBrushAlertBackgroundActive = 3, kThemeBrushAlertBackgroundInactive = 4,
    kThemeBrushModelessDialogBackgroundActive = 5, kThemeBrushModelessDialogBackgroundInactive = 6,
    kThemeBrushUtilityWindowBackgroundActive = 7, kThemeBrushUtilityWindowBackgroundInactive = 8,
    kThemeBrushListViewSortColumnBackground = 9, kThemeBrushListViewBackground = 10,
    kThemeBrushIconLabelBackground = 11, kThemeBrushListViewSeparator = 12,
    kThemeBrushChasingArrows = 13, kThemeBrushDragHilite = 14,
    kThemeBrushDocumentWindowBackground = 15, kThemeBrushFinderWindowBackground = 16,
    kThemeBrushScrollBarDelimiterActive = 17, kThemeBrushScrollBarDelimiterInactive = 18,
    kThemeBrushFocusHighlight = 19, kThemeBrushPopupArrowActive = 20,
    kThemeBrushPopupArrowPressed = 21, kThemeBrushPopupArrowInactive = 22,
    kThemeBrushAppleGuideCoachmark = 23, kThemeBrushIconLabelBackgroundSelected = 24,
    kThemeBrushStaticAreaFill = 25, kThemeBrushActiveAreaFill = 26,
    kThemeBrushButtonFrameActive = 27, kThemeBrushButtonFrameInactive = 28,
    kThemeBrushButtonFaceActive = 29, kThemeBrushButtonFaceInactive = 30,
    kThemeBrushButtonFacePressed = 31, kThemeBrushButtonActiveDarkShadow = 32,
    kThemeBrushButtonActiveDarkHighlight = 33, kThemeBrushButtonActiveLightShadow = 34,
    kThemeBrushButtonActiveLightHighlight = 35, kThemeBrushButtonInactiveDarkShadow = 36,
    kThemeBrushButtonInactiveDarkHighlight = 37, kThemeBrushButtonInactiveLightShadow = 38,
    kThemeBrushButtonInactiveLightHighlight = 39, kThemeBrushButtonPressedDarkShadow = 40,
    kThemeBrushButtonPressedDarkHighlight = 41, kThemeBrushButtonPressedLightShadow = 42,
    kThemeBrushButtonPressedLightHighlight = 43, kThemeBrushBevelActiveLight = 44,
    kThemeBrushBevelActiveDark = 45, kThemeBrushBevelInactiveLight = 46,
    kThemeBrushBevelInactiveDark = 47, kThemeBrushNotificationWindowBackground = 48,
    kThemeBrushMovableModalBackground = 49, kThemeBrushSheetBackgroundOpaque = 50,
    kThemeBrushDrawerBackground = 51, kThemeBrushToolbarBackground = 52,
    kThemeBrushSheetBackgroundTransparent = 53, kThemeBrushMenuBackground = 54,
    kThemeBrushMenuBackgroundSelected = 55, kThemeBrushListViewOddRowBackground = 56,
    kThemeBrushListViewEvenRowBackground = 57, kThemeBrushListViewColumnDivider = 58,
    kThemeBrushSheetBackground = kThemeBrushSheetBackgroundOpaque,
    kThemeBrushBlack = -1, kThemeBrushWhite = -2,
    kThemeBrushPrimaryHighlightColor = -3, kThemeBrushSecondaryHighlightColor = -4,
    kThemeBrushAlternatePrimaryHighlightColor = -5
};

typedef SInt16 ThemeTextColor;
enum {
    kThemeTextColorDialogActive = 1, kThemeTextColorDialogInactive = 2,
    kThemeTextColorAlertActive = 3, kThemeTextColorAlertInactive = 4,
    kThemeTextColorModelessDialogActive = 5, kThemeTextColorModelessDialogInactive = 6,
    kThemeTextColorWindowHeaderActive = 7, kThemeTextColorWindowHeaderInactive = 8,
    kThemeTextColorPlacardActive = 9, kThemeTextColorPlacardInactive = 10,
    kThemeTextColorPlacardPressed = 11, kThemeTextColorPushButtonActive = 12,
    kThemeTextColorPushButtonInactive = 13, kThemeTextColorPushButtonPressed = 14,
    kThemeTextColorBevelButtonActive = 15, kThemeTextColorBevelButtonInactive = 16,
    kThemeTextColorBevelButtonPressed = 17, kThemeTextColorPopupButtonActive = 18,
    kThemeTextColorPopupButtonInactive = 19, kThemeTextColorPopupButtonPressed = 20,
    kThemeTextColorIconLabel = 21, kThemeTextColorListView = 22,
    kThemeTextColorDocumentWindowTitleActive = 23, kThemeTextColorDocumentWindowTitleInactive = 24,
    kThemeTextColorMovableModalWindowTitleActive = 25, kThemeTextColorMovableModalWindowTitleInactive = 26,
    kThemeTextColorUtilityWindowTitleActive = 27, kThemeTextColorUtilityWindowTitleInactive = 28,
    kThemeTextColorPopupWindowTitleActive = 29, kThemeTextColorPopupWindowTitleInactive = 30,
    kThemeTextColorRootMenuActive = 31, kThemeTextColorRootMenuSelected = 32,
    kThemeTextColorRootMenuDisabled = 33, kThemeTextColorMenuItemActive = 34,
    kThemeTextColorMenuItemSelected = 35, kThemeTextColorMenuItemDisabled = 36,
    kThemeTextColorPopupLabelActive = 37, kThemeTextColorPopupLabelInactive = 38,
    kThemeTextColorTabFrontActive = 39, kThemeTextColorTabNonFrontActive = 40,
    kThemeTextColorTabNonFrontPressed = 41, kThemeTextColorTabFrontInactive = 42,
    kThemeTextColorTabNonFrontInactive = 43, kThemeTextColorIconLabelSelected = 44,
    kThemeTextColorBevelButtonStickyActive = 45, kThemeTextColorBevelButtonStickyInactive = 46,
    kThemeTextColorNotification = 47, kThemeTextColorSystemDetail = 48,
    kThemeTextColorBlack = -1, kThemeTextColorWhite = -2
};

OSStatus HIThemeBrushCreateCGColor(ThemeBrush inBrush, CGColorRef *outColor);
OSStatus HIThemeSetFill(ThemeBrush inBrush, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
OSStatus HIThemeSetStroke(ThemeBrush inBrush, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
OSStatus HIThemeSetTextFill(ThemeTextColor inColor, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation);
OSStatus GetThemeTextColor(ThemeTextColor inColor, SInt16 inDepth, Boolean inIsColorDevice, RGBColor *outColor);

/* ---- Buttons ---- */

typedef UInt16 ThemeButtonKind;
enum {
    kThemePushButton = 0, kThemeCheckBox = 1, kThemeRadioButton = 2, kThemeBevelButton = 3,
    kThemeArrowButton = 4, kThemePopupButton = 5, kThemeDisclosureButton = 6,
    kThemeIncDecButton = 7, kThemeSmallBevelButton = 8, kThemeMediumBevelButton = 3,
    kThemeLargeBevelButton = 9, kThemeListHeaderButton = 10, kThemeRoundButton = 11,
    kThemeLargeRoundButton = 12, kThemeSmallCheckBox = 13, kThemeSmallRadioButton = 14,
    kThemeRoundedBevelButton = 15, kThemeComboBox = 16, kThemeComboBoxSmall = 17,
    kThemeComboBoxMini = 18, kThemeMiniCheckBox = 19, kThemeMiniRadioButton = 20,
    kThemeIncDecButtonSmall = 21, kThemeIncDecButtonMini = 22, kThemeArrowButtonSmall = 23,
    kThemeArrowButtonMini = 24, kThemePushButtonNormal = 25, kThemePushButtonSmall = 26,
    kThemePushButtonMini = 27, kThemePopupButtonNormal = 28, kThemePopupButtonSmall = 29,
    kThemePopupButtonMini = 30, kThemeBevelButtonInset = 31, kThemePushButtonInset = 32,
    kThemePushButtonInsetSmall = 33, kThemeRoundButtonHelp = 34, kThemeCheckBoxMini = kThemeMiniCheckBox,
    kThemeDisclosureTriangle = kThemeDisclosureButton
};

typedef UInt16 ThemeButtonValue;
enum {
    kThemeButtonOff = 0, kThemeButtonOn = 1, kThemeButtonMixed = 2,
    kThemeDisclosureRight = 0, kThemeDisclosureDown = 1, kThemeDisclosureLeft = 2
};

typedef UInt16 ThemeButtonAdornment;
enum {
    kThemeAdornmentNone = 0, kThemeAdornmentDefault = (1 << 0), kThemeAdornmentFocus = (1 << 2),
    kThemeAdornmentRightToLeft = (1 << 4), kThemeAdornmentDrawIndicatorOnly = (1 << 5),
    kThemeAdornmentHeaderButtonLeftNeighborSelected = (1 << 6),
    kThemeAdornmentHeaderButtonRightNeighborSelected = (1 << 7),
    kThemeAdornmentHeaderButtonSortUp = (1 << 8), kThemeAdornmentHeaderMenuButton = (1 << 9),
    kThemeAdornmentHeaderButtonNoShadow = (1 << 10), kThemeAdornmentHeaderButtonShadowOnly = (1 << 11),
    kThemeAdornmentHeaderButtonNoSortArrow = (1 << 12), kThemeAdornmentArrowLeftArrow = (1 << 6),
    kThemeAdornmentArrowDownArrow = (1 << 7), kThemeAdornmentArrowDoubleArrow = (1 << 8),
    kThemeAdornmentArrowUpArrow = (1 << 9)
};

typedef struct {
    UInt32 version;
    ThemeDrawState state;
    ThemeButtonKind kind;
    ThemeButtonValue value;
    ThemeButtonAdornment adornment;
    union {
        struct { CFAbsoluteTime start; CFAbsoluteTime current; } time;
        struct { UInt32 index; } frame;
    } animation;
} HIThemeButtonDrawInfo;

OSStatus HIThemeDrawButton(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation, HIRect *outLabelRect);
OSStatus HIThemeGetButtonContentBounds(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    HIRect *outBounds);
OSStatus HIThemeGetButtonBackgroundBounds(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    HIRect *outBounds);

/* ---- Tracks: scroll bars, sliders, progress bars ---- */

typedef UInt16 ThemeTrackKind;
enum {
    kThemeScrollBarMedium = 0, kThemeScrollBarSmall = 1, kThemeSliderMedium = 2,
    kThemeProgressBarMedium = 3, kThemeIndeterminateBarMedium = 4, kThemeRelevanceBar = 5,
    kThemeSliderSmall = 6, kThemeProgressBarLarge = 7, kThemeIndeterminateBarLarge = 8,
    kThemeScrollBarMini = 9, kThemeSliderMini = 10, kThemeProgressBarMini = 11,
    kThemeIndeterminateBarMini = 12,
    kThemeScrollBar = kThemeScrollBarMedium, kThemeSlider = kThemeSliderMedium,
    kThemeProgressBar = kThemeProgressBarMedium, kThemeIndeterminateBar = kThemeIndeterminateBarMedium
};
typedef UInt16 ThemeTrackAttributes;
enum {
    kThemeTrackHorizontal = (1 << 0), kThemeTrackRightToLeft = (1 << 1),
    kThemeTrackShowThumb = (1 << 2), kThemeTrackThumbRgnIsNotGhost = (1 << 3),
    kThemeTrackNoScrollBarArrows = (1 << 4), kThemeTrackHasFocus = (1 << 5)
};
typedef UInt8 ThemeTrackEnableState;
enum { kThemeTrackActive = 0, kThemeTrackDisabled = 1, kThemeTrackNothingToScroll = 2, kThemeTrackInactive = 3 };
typedef UInt8 ThemeTrackPressState;
enum {
    kThemeLeftOutsideArrowPressed = 0x01, kThemeLeftInsideArrowPressed = 0x02,
    kThemeLeftTrackPressed = 0x04, kThemeThumbPressed = 0x08, kThemeRightTrackPressed = 0x10,
    kThemeRightInsideArrowPressed = 0x20, kThemeRightOutsideArrowPressed = 0x40,
    kThemeTopOutsideArrowPressed = kThemeLeftOutsideArrowPressed,
    kThemeTopInsideArrowPressed = kThemeLeftInsideArrowPressed,
    kThemeTopTrackPressed = kThemeLeftTrackPressed,
    kThemeBottomTrackPressed = kThemeRightTrackPressed,
    kThemeBottomInsideArrowPressed = kThemeRightInsideArrowPressed,
    kThemeBottomOutsideArrowPressed = kThemeRightOutsideArrowPressed
};
typedef UInt8 ThemeThumbDirection;
enum { kThemeThumbPlain = 0, kThemeThumbUpward = 1, kThemeThumbDownward = 2 };
enum { kThemeTrackHideTrack = (1 << 6) };

typedef struct { SInt32 viewsize; ThemeTrackPressState pressState; } ScrollBarTrackInfo;
typedef struct { ThemeThumbDirection thumbDir; ThemeTrackPressState pressState; } SliderTrackInfo;
typedef struct { UInt8 phase; } ProgressTrackInfo;

typedef struct {
    UInt32 version;
    ThemeTrackKind kind;
    HIRect bounds;
    SInt32 min;
    SInt32 max;
    SInt32 value;
    UInt32 reserved;
    ThemeTrackAttributes attributes;
    ThemeTrackEnableState enableState;
    UInt8 filler1;
    union {
        ScrollBarTrackInfo scrollbar;
        SliderTrackInfo slider;
        ProgressTrackInfo progress;
    } trackInfo;
} HIThemeTrackDrawInfo;

OSStatus HIThemeDrawTrack(const HIThemeTrackDrawInfo *inDrawInfo, const HIRect *inGhostRect,
    CGContextRef inContext, HIThemeOrientation inOrientation);

/* ---- Tabs ---- */

typedef UInt16 ThemeTabStyle;
enum {
    kThemeTabNonFront = 0, kThemeTabNonFrontPressed = 1, kThemeTabNonFrontInactive = 2,
    kThemeTabFront = 3, kThemeTabFrontInactive = 4, kThemeTabNonFrontUnavailable = 5,
    kThemeTabFrontUnavailable = 6
};
typedef UInt16 ThemeTabDirection;
enum { kThemeTabNorth = 0, kThemeTabSouth = 1, kThemeTabEast = 2, kThemeTabWest = 3 };
typedef UInt32 HIThemeTabSize;
enum { kHIThemeTabSizeNormal = 0, kHIThemeTabSizeSmall = 1, kHIThemeTabSizeMini = 3 };
typedef UInt32 HIThemeTabAdornment;
enum {
    kHIThemeTabAdornmentNone = 0, kHIThemeTabAdornmentFocus = (1 << 2),
    kHIThemeTabAdornmentLeadingSeparator = (1 << 3), kHIThemeTabAdornmentTrailingSeparator = (1 << 4)
};
typedef UInt32 HIThemeTabKind;
enum { kHIThemeTabKindNormal = 0 };
typedef UInt32 HIThemeTabPosition;
enum {
    kHIThemeTabPositionFirst = 0, kHIThemeTabPositionMiddle = 1, kHIThemeTabPositionLast = 2,
    kHIThemeTabPositionOnly = 3
};
typedef UInt32 HIThemeTabPaneAdornment;
enum { kHIThemeTabPaneAdornmentNormal = 0 };

typedef struct {
    UInt32 version;
    ThemeTabStyle style;
    ThemeTabDirection direction;
    HIThemeTabSize size;
    HIThemeTabAdornment adornment;
    HIThemeTabKind kind;
    HIThemeTabPosition position;
} HIThemeTabDrawInfo;

typedef struct {
    UInt32 version;
    ThemeDrawState state;
    ThemeTabDirection direction;
    HIThemeTabSize size;
    HIThemeTabKind kind;
    HIThemeTabPaneAdornment adornment;
} HIThemeTabPaneDrawInfo;

OSStatus HIThemeDrawTab(const HIRect *inRect, const HIThemeTabDrawInfo *inDrawInfo, CGContextRef inContext,
    HIThemeOrientation inOrientation, HIRect *outLabelRect);
OSStatus HIThemeDrawTabPane(const HIRect *inRect, const HIThemeTabPaneDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation);

/* ---- Frames, group boxes, separators, the grow box, backgrounds ---- */

typedef UInt32 HIThemeFrameKind;
enum { kHIThemeFrameTextFieldSquare = 0, kHIThemeFrameListBox = 1 };
typedef struct {
    UInt32 version;
    HIThemeFrameKind kind;
    ThemeDrawState state;
    Boolean isFocused;
} HIThemeFrameDrawInfo;
OSStatus HIThemeDrawFrame(const HIRect *inRect, const HIThemeFrameDrawInfo *inDrawInfo, CGContextRef inContext,
    HIThemeOrientation inOrientation);

typedef UInt32 HIThemeGroupBoxKind;
enum { kHIThemeGroupBoxKindPrimary = 0, kHIThemeGroupBoxKindSecondary = 1,
       kHIThemeGroupBoxKindPrimaryOpaque = 3, kHIThemeGroupBoxKindSecondaryOpaque = 4 };
typedef struct {
    UInt32 version;
    ThemeDrawState state;
    HIThemeGroupBoxKind kind;
} HIThemeGroupBoxDrawInfo;
OSStatus HIThemeDrawGroupBox(const HIRect *inRect, const HIThemeGroupBoxDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation);

typedef struct {
    UInt32 version;
    ThemeDrawState state;
} HIThemeSeparatorDrawInfo;
OSStatus HIThemeDrawSeparator(const HIRect *inRect, const HIThemeSeparatorDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation);

typedef UInt32 HIThemeGrowBoxKind;
enum { kHIThemeGrowBoxKindNormal = 0, kHIThemeGrowBoxKindNone = 1 };
typedef UInt32 HIThemeGrowBoxSize;
enum { kHIThemeGrowBoxSizeNormal = 0, kHIThemeGrowBoxSizeSmall = 1 };
typedef UInt16 ThemeGrowDirection;
enum { kThemeGrowLeft = (1 << 0), kThemeGrowRight = (1 << 1), kThemeGrowUp = (1 << 2), kThemeGrowDown = (1 << 3) };
typedef struct {
    UInt32 version;
    ThemeDrawState state;
    HIThemeGrowBoxKind kind;
    ThemeGrowDirection direction;
    HIThemeGrowBoxSize size;
} HIThemeGrowBoxDrawInfo;
OSStatus HIThemeDrawGrowBox(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation);
OSStatus HIThemeGetGrowBoxBounds(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo,
    HIRect *outBounds);

typedef UInt32 ThemeBackgroundKind;
enum {
    kThemeBackgroundTabPane = 1, kThemeBackgroundPlacard = 2, kThemeBackgroundWindowHeader = 3,
    kThemeBackgroundListViewWindowHeader = 4, kThemeBackgroundSecondaryGroupBox = 5,
    kThemeBackgroundMetal = 6
};
typedef struct {
    UInt32 version;
    ThemeDrawState state;
    ThemeBackgroundKind kind;
} HIThemeBackgroundDrawInfo;
OSStatus HIThemeApplyBackground(const HIRect *inBounds, const HIThemeBackgroundDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation);

#ifdef __cplusplus
}
#endif

#endif
