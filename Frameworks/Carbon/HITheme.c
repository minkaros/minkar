/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Appearance Manager and HITheme in Atlas's look (its light
 * appearance): plain shapes drawn with CoreGraphics, from the documented
 * API alone. The colours are Atlas's greys and its blue accent.
 */

#include <Carbon/HITheme.h>
#include <CoreServices/CoreServices.h>
#include <math.h>

/* ---- Colours ---- */

typedef struct { CGFloat r, g, b, a; } Colour;

static const Colour kWindow = { 0.925, 0.925, 0.925, 1 };
static const Colour kFace = { 0.97, 0.97, 0.97, 1 };
static const Colour kFacePressed = { 0.82, 0.82, 0.82, 1 };
static const Colour kFrame = { 0.65, 0.65, 0.65, 1 };
static const Colour kAccent = { 0.15, 0.40, 0.85, 1 };
static const Colour kAccentLight = { 0.70, 0.80, 0.95, 1 };
static const Colour kText = { 0.0, 0.0, 0.0, 1 };
static const Colour kTextDisabled = { 0.55, 0.55, 0.55, 1 };
static const Colour kWhite = { 1, 1, 1, 1 };
static const Colour kBlack = { 0, 0, 0, 1 };
static const Colour kTrack = { 0.88, 0.88, 0.88, 1 };
static const Colour kThumb = { 0.62, 0.62, 0.62, 1 };

static Colour brushColour(ThemeBrush brush) {
    switch (brush) {
    case kThemeBrushBlack: return kBlack;
    case kThemeBrushWhite: return kWhite;
    case kThemeBrushPrimaryHighlightColor:
    case kThemeBrushAlternatePrimaryHighlightColor:
    case kThemeBrushMenuBackgroundSelected:
    case kThemeBrushIconLabelBackgroundSelected:
    case kThemeBrushFocusHighlight: return kAccent;
    case kThemeBrushSecondaryHighlightColor:
    case kThemeBrushDragHilite: return kAccentLight;
    case kThemeBrushListViewBackground:
    case kThemeBrushListViewEvenRowBackground:
    case kThemeBrushIconLabelBackground:
    case kThemeBrushMenuBackground: return kWhite;
    case kThemeBrushListViewOddRowBackground: return (Colour){ 0.96, 0.96, 0.96, 1 };
    case kThemeBrushListViewSortColumnBackground: return (Colour){ 0.94, 0.94, 0.94, 1 };
    case kThemeBrushListViewSeparator:
    case kThemeBrushListViewColumnDivider:
    case kThemeBrushScrollBarDelimiterActive:
    case kThemeBrushScrollBarDelimiterInactive:
    case kThemeBrushButtonFrameActive:
    case kThemeBrushButtonFrameInactive: return kFrame;
    case kThemeBrushButtonFaceActive:
    case kThemeBrushButtonFaceInactive: return kFace;
    case kThemeBrushButtonFacePressed: return kFacePressed;
    case kThemeBrushButtonActiveDarkShadow:
    case kThemeBrushButtonInactiveDarkShadow:
    case kThemeBrushButtonPressedDarkShadow:
    case kThemeBrushBevelActiveDark:
    case kThemeBrushBevelInactiveDark: return (Colour){ 0.55, 0.55, 0.55, 1 };
    case kThemeBrushButtonActiveLightShadow:
    case kThemeBrushButtonInactiveLightShadow:
    case kThemeBrushButtonPressedLightShadow: return (Colour){ 0.75, 0.75, 0.75, 1 };
    case kThemeBrushButtonActiveDarkHighlight:
    case kThemeBrushButtonInactiveDarkHighlight:
    case kThemeBrushButtonPressedDarkHighlight: return (Colour){ 0.90, 0.90, 0.90, 1 };
    case kThemeBrushButtonActiveLightHighlight:
    case kThemeBrushButtonInactiveLightHighlight:
    case kThemeBrushButtonPressedLightHighlight:
    case kThemeBrushBevelActiveLight:
    case kThemeBrushBevelInactiveLight: return kWhite;
    case kThemeBrushPopupArrowActive:
    case kThemeBrushPopupArrowPressed:
    case kThemeBrushChasingArrows: return kText;
    case kThemeBrushPopupArrowInactive: return kTextDisabled;
    case kThemeBrushSheetBackgroundTransparent: return (Colour){ 0.925, 0.925, 0.925, 0.9 };
    case kThemeBrushAppleGuideCoachmark: return (Colour){ 0.95, 0.30, 0.30, 1 };
    default: return kWindow;   /* the dialog, document, utility, toolbar and other backgrounds */
    }
}

static Colour textColour(ThemeTextColor color) {
    switch (color) {
    case kThemeTextColorBlack: return kBlack;
    case kThemeTextColorWhite:
    case kThemeTextColorRootMenuSelected:
    case kThemeTextColorMenuItemSelected:
    case kThemeTextColorIconLabelSelected: return kWhite;
    case kThemeTextColorDialogInactive:
    case kThemeTextColorAlertInactive:
    case kThemeTextColorModelessDialogInactive:
    case kThemeTextColorWindowHeaderInactive:
    case kThemeTextColorPlacardInactive:
    case kThemeTextColorPushButtonInactive:
    case kThemeTextColorBevelButtonInactive:
    case kThemeTextColorPopupButtonInactive:
    case kThemeTextColorDocumentWindowTitleInactive:
    case kThemeTextColorMovableModalWindowTitleInactive:
    case kThemeTextColorUtilityWindowTitleInactive:
    case kThemeTextColorPopupWindowTitleInactive:
    case kThemeTextColorRootMenuDisabled:
    case kThemeTextColorMenuItemDisabled:
    case kThemeTextColorPopupLabelInactive:
    case kThemeTextColorTabFrontInactive:
    case kThemeTextColorTabNonFrontInactive:
    case kThemeTextColorBevelButtonStickyInactive: return kTextDisabled;
    case kThemeTextColorSystemDetail: return (Colour){ 0.45, 0.45, 0.45, 1 };
    default: return kText;
    }
}

static void setFill(CGContextRef c, Colour k) { CGContextSetRGBFillColor(c, k.r, k.g, k.b, k.a); }
static void setStroke(CGContextRef c, Colour k) { CGContextSetRGBStrokeColor(c, k.r, k.g, k.b, k.a); }

OSStatus HIThemeBrushCreateCGColor(ThemeBrush inBrush, CGColorRef *outColor) {
    if (outColor == NULL)
        return paramErr;
    Colour k = brushColour(inBrush);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[4] = { k.r, k.g, k.b, k.a };
    *outColor = CGColorCreate(space, comps);
    CGColorSpaceRelease(space);
    return *outColor != NULL ? noErr : memFullErr;
}

OSStatus HIThemeSetFill(ThemeBrush inBrush, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inContext == NULL)
        return paramErr;
    setFill(inContext, brushColour(inBrush));
    return noErr;
}

OSStatus HIThemeSetStroke(ThemeBrush inBrush, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inContext == NULL)
        return paramErr;
    setStroke(inContext, brushColour(inBrush));
    return noErr;
}

OSStatus HIThemeSetTextFill(ThemeTextColor inColor, void *inInfo, CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inContext == NULL)
        return paramErr;
    setFill(inContext, textColour(inColor));
    return noErr;
}

OSStatus GetThemeTextColor(ThemeTextColor inColor, SInt16 inDepth, Boolean inIsColorDevice, RGBColor *outColor) {
    if (outColor == NULL)
        return paramErr;
    Colour k = textColour(inColor);
    outColor->red = (unsigned short)lround(k.r * 65535);
    outColor->green = (unsigned short)lround(k.g * 65535);
    outColor->blue = (unsigned short)lround(k.b * 65535);
    return noErr;
}

/* ---- Metrics and fonts ---- */

OSStatus GetThemeMetric(ThemeMetric inMetric, SInt32 *outMetric) {
    static const SInt32 metrics[kThemeMetricCount] = {
        [kThemeMetricScrollBarWidth] = 15, [kThemeMetricSmallScrollBarWidth] = 11,
        [kThemeMetricCheckBoxHeight] = 16, [kThemeMetricRadioButtonHeight] = 16,
        [kThemeMetricEditTextWhitespace] = 2, [kThemeMetricEditTextFrameOutset] = 1,
        [kThemeMetricListBoxFrameOutset] = 1, [kThemeMetricFocusRectOutset] = 3,
        [kThemeMetricImageWellThickness] = 3, [kThemeMetricScrollBarOverlap] = 0,
        [kThemeMetricLargeTabHeight] = 24, [kThemeMetricLargeTabCapsWidth] = 8,
        [kThemeMetricTabFrameOverlap] = 1, [kThemeMetricTabIndentOrStyle] = 0,
        [kThemeMetricTabOverlap] = 0, [kThemeMetricSmallTabHeight] = 20,
        [kThemeMetricSmallTabCapsWidth] = 6, [kThemeMetricPushButtonHeight] = 22,
        [kThemeMetricListHeaderHeight] = 20, [kThemeMetricDisclosureTriangleHeight] = 13,
        [kThemeMetricDisclosureTriangleWidth] = 13, [kThemeMetricLittleArrowsHeight] = 22,
        [kThemeMetricLittleArrowsWidth] = 13, [kThemeMetricPopupButtonHeight] = 22,
        [kThemeMetricSmallPopupButtonHeight] = 19, [kThemeMetricLargeProgressBarThickness] = 16,
        [kThemeMetricPullDownHeight] = 22, [kThemeMetricSmallPullDownHeight] = 19,
        [kThemeMetricResizeControlHeight] = 15, [kThemeMetricSmallResizeControlHeight] = 11,
        [kThemeMetricHSliderHeight] = 20, [kThemeMetricVSliderWidth] = 20,
        [kThemeMetricCheckBoxWidth] = 16, [kThemeMetricRadioButtonWidth] = 16,
        [kThemeMetricNormalProgressBarThickness] = 12, [kThemeMetricProgressBarShadowOutset] = 1,
        [kThemeMetricMenuMarkColumnWidth] = 18, [kThemeMetricMenuExcludedMarkColumnWidth] = 18,
        [kThemeMetricMenuMarkIndent] = 4, [kThemeMetricMenuTextLeadingEdgeMargin] = 6,
        [kThemeMetricMenuTextTrailingEdgeMargin] = 12, [kThemeMetricMenuIndentWidth] = 12,
        [kThemeMetricMenuIconTrailingEdgeMargin] = 4, [kThemeMetricSmallCheckBoxHeight] = 14,
        [kThemeMetricSmallCheckBoxWidth] = 14, [kThemeMetricSmallRadioButtonHeight] = 14,
        [kThemeMetricSmallRadioButtonWidth] = 14,
    };
    if (outMetric == NULL)
        return paramErr;
    if (inMetric >= kThemeMetricCount) {
        *outMetric = 0;
        return themeInvalidMetricErr;
    }
    *outMetric = metrics[inMetric];
    return noErr;
}

OSStatus GetThemeMenuSeparatorHeight(SInt16 *outHeight) {
    if (outHeight == NULL)
        return paramErr;
    *outHeight = 11;
    return noErr;
}

OSStatus GetThemeMenuItemExtra(ThemeMenuItemType inItemType, SInt16 *outHeight, SInt16 *outWidth) {
    if (outHeight != NULL)
        *outHeight = 4;
    if (outWidth != NULL)
        *outWidth = 30;
    return noErr;
}

CTFontUIFontType HIThemeGetUIFontType(ThemeFontID inFontID) {
    switch (inFontID) {
    case kThemeSystemFont: return kCTFontUIFontSystem;
    case kThemeSmallSystemFont: return kCTFontUIFontSmallSystem;
    case kThemeSmallEmphasizedSystemFont: return kCTFontUIFontSmallEmphasizedSystem;
    case kThemeViewsFont: return kCTFontUIFontViews;
    case kThemeEmphasizedSystemFont: return kCTFontUIFontEmphasizedSystem;
    case kThemeApplicationFont: return kCTFontUIFontApplication;
    case kThemeLabelFont: return kCTFontUIFontLabel;
    case kThemeMenuTitleFont: return kCTFontUIFontMenuTitle;
    case kThemeMenuItemFont: return kCTFontUIFontMenuItem;
    case kThemeMenuItemMarkFont: return kCTFontUIFontMenuItemMark;
    case kThemeMenuItemCmdKeyFont: return kCTFontUIFontMenuItemCmdKey;
    case kThemeWindowTitleFont: return kCTFontUIFontWindowTitle;
    case kThemePushButtonFont: return kCTFontUIFontPushButton;
    case kThemeUtilityWindowTitleFont: return kCTFontUIFontUtilityWindowTitle;
    case kThemeAlertHeaderFont: return kCTFontUIFontAlertHeader;
    case kThemeToolbarFont: return kCTFontUIFontToolbar;
    case kThemeMiniSystemFont: return kCTFontUIFontMiniSystem;
    case kThemeSystemFontDetail: return kCTFontUIFontSystemDetail;
    case kThemeSystemFontDetailEmphasized: return kCTFontUIFontEmphasizedSystemDetail;
    default: return kCTFontUIFontNone;
    }
}

/* ---- Shapes ---- */

static void roundedRect(CGContextRef c, CGRect r, CGFloat radius) {
    CGPathRef path = CGPathCreateWithRoundedRect(r, radius, radius, NULL);
    CGContextAddPath(c, path);
    CGPathRelease(path);
}

static void fillAndFrame(CGContextRef c, CGRect r, CGFloat radius, Colour face, Colour frame, CGFloat alpha) {
    CGRect inner = CGRectInset(r, 0.5, 0.5);
    if (inner.size.width <= 0 || inner.size.height <= 0)
        return;
    radius = fmin(radius, fmin(inner.size.width, inner.size.height) / 2);
    face.a *= alpha;
    frame.a *= alpha;
    setFill(c, face);
    roundedRect(c, inner, radius);
    CGContextFillPath(c);
    setStroke(c, frame);
    CGContextSetLineWidth(c, 1.0);
    roundedRect(c, inner, radius);
    CGContextStrokePath(c);
}

static Boolean stateActive(ThemeDrawState state) {
    return state != kThemeStateInactive && state != kThemeStateUnavailable &&
           state != kThemeStateUnavailableInactive;
}

static Boolean statePressed(ThemeDrawState state) {
    return state == kThemeStatePressed || state == kThemeStatePressedDown;
}

/* A small triangle pointing down (or right), centred in r. */
static void triangle(CGContextRef c, CGRect r, Boolean down, Colour k) {
    CGFloat cx = CGRectGetMidX(r), cy = CGRectGetMidY(r), s = fmin(r.size.width, r.size.height) * 0.3;
    setFill(c, k);
    CGContextBeginPath(c);
    if (down) {
        CGContextMoveToPoint(c, cx - s, cy - s / 2);
        CGContextAddLineToPoint(c, cx + s, cy - s / 2);
        CGContextAddLineToPoint(c, cx, cy + s / 2);
    } else {
        CGContextMoveToPoint(c, cx - s / 2, cy - s);
        CGContextAddLineToPoint(c, cx + s / 2, cy);
        CGContextAddLineToPoint(c, cx - s / 2, cy + s);
    }
    CGContextClosePath(c);
    CGContextFillPath(c);
}

/* ---- Buttons ---- */

static Boolean isCheck(ThemeButtonKind k) {
    return k == kThemeCheckBox || k == kThemeSmallCheckBox || k == kThemeMiniCheckBox;
}

static Boolean isRadio(ThemeButtonKind k) {
    return k == kThemeRadioButton || k == kThemeSmallRadioButton || k == kThemeMiniRadioButton;
}

static Boolean isPopup(ThemeButtonKind k) {
    return k == kThemePopupButton || k == kThemePopupButtonNormal || k == kThemePopupButtonSmall ||
           k == kThemePopupButtonMini || k == kThemeComboBox || k == kThemeComboBoxSmall || k == kThemeComboBoxMini;
}

static Boolean isDisclosure(ThemeButtonKind k) { return k == kThemeDisclosureButton; }

static Boolean isIncDec(ThemeButtonKind k) {
    return k == kThemeIncDecButton || k == kThemeIncDecButtonSmall || k == kThemeIncDecButtonMini;
}

/* The indicator's square: at the left of the bounds, its height. */
static CGRect indicatorRect(CGRect b, ThemeButtonKind k) {
    CGFloat size = (k == kThemeSmallCheckBox || k == kThemeSmallRadioButton) ? 14 :
                   (k == kThemeMiniCheckBox || k == kThemeMiniRadioButton) ? 12 : 16;
    size = fmin(size, b.size.height);
    return CGRectMake(b.origin.x, CGRectGetMidY(b) - size / 2, size, size);
}

OSStatus HIThemeGetButtonContentBounds(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo, HIRect *outBounds) {
    if (inBounds == NULL || inDrawInfo == NULL || outBounds == NULL)
        return paramErr;
    CGRect b = *inBounds;
    ThemeButtonKind k = inDrawInfo->kind;
    if (isCheck(k) || isRadio(k)) {
        CGRect ind = indicatorRect(b, k);
        *outBounds = CGRectMake(CGRectGetMaxX(ind) + 4, b.origin.y, fmax(0, CGRectGetMaxX(b) - CGRectGetMaxX(ind) - 4), b.size.height);
    } else if (isPopup(k)) {
        *outBounds = CGRectMake(b.origin.x + 8, b.origin.y + 2, fmax(0, b.size.width - 8 - 22), fmax(0, b.size.height - 4));
    } else {
        CGFloat r = b.size.height < 12 ? b.size.height / 2 : 6;
        *outBounds = CGRectInset(b, r, 2);
    }
    return noErr;
}

OSStatus HIThemeGetButtonBackgroundBounds(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo, HIRect *outBounds) {
    if (inBounds == NULL || inDrawInfo == NULL || outBounds == NULL)
        return paramErr;
    *outBounds = *inBounds;
    return noErr;
}

OSStatus HIThemeDrawButton(const HIRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation, HIRect *outLabelRect) {
    if (inBounds == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    CGRect b = *inBounds;
    ThemeButtonKind k = inDrawInfo->kind;
    Boolean active = stateActive(inDrawInfo->state);
    Boolean pressed = statePressed(inDrawInfo->state);
    CGFloat alpha = active ? 1.0 : 0.5;
    Boolean on = inDrawInfo->value == kThemeButtonOn, mixed = inDrawInfo->value == kThemeButtonMixed;

    CGContextSaveGState(inContext);
    if (isDisclosure(k)) {
        Colour t = kText;
        t.a = alpha;
        triangle(inContext, b, inDrawInfo->value == kThemeDisclosureDown, t);
    } else if (isCheck(k) || isRadio(k)) {
        CGRect ind = indicatorRect(b, k);
        Boolean filled = on || mixed;
        fillAndFrame(inContext, ind, isRadio(k) ? ind.size.width / 2 : 3,
            filled ? kAccent : (pressed ? kFacePressed : kWhite), filled ? kAccent : kFrame, alpha);
        CGContextSetRGBStrokeColor(inContext, 1, 1, 1, alpha);
        CGContextSetRGBFillColor(inContext, 1, 1, 1, alpha);
        CGFloat x = ind.origin.x, y = ind.origin.y, w = ind.size.width, h = ind.size.height;
        if (mixed) {
            CGContextFillRect(inContext, CGRectMake(x + w * 0.25, y + h / 2 - 1, w * 0.5, 2));
        } else if (on && isRadio(k)) {
            CGContextFillEllipseInRect(inContext, CGRectInset(ind, w * 0.33, h * 0.33));
        } else if (on) {
            CGContextSetLineWidth(inContext, 2.0);
            CGContextMoveToPoint(inContext, x + w * 0.25, y + h * 0.5);
            CGContextAddLineToPoint(inContext, x + w * 0.45, y + h * 0.7);
            CGContextAddLineToPoint(inContext, x + w * 0.75, y + h * 0.3);
            CGContextStrokePath(inContext);
        }
    } else if (isIncDec(k)) {
        fillAndFrame(inContext, b, 4, pressed ? kFacePressed : kFace, kFrame, alpha);
        CGRect top = CGRectMake(b.origin.x, b.origin.y, b.size.width, b.size.height / 2);
        CGRect bottom = CGRectMake(b.origin.x, CGRectGetMidY(b), b.size.width, b.size.height / 2);
        Colour t = kText;
        t.a = alpha;
        setFill(inContext, t);
        CGFloat cx = CGRectGetMidX(b), s = fmin(b.size.width, b.size.height / 2) * 0.25;
        CGContextMoveToPoint(inContext, cx - s, CGRectGetMidY(top) + s / 2);
        CGContextAddLineToPoint(inContext, cx + s, CGRectGetMidY(top) + s / 2);
        CGContextAddLineToPoint(inContext, cx, CGRectGetMidY(top) - s / 2);
        CGContextClosePath(inContext);
        CGContextMoveToPoint(inContext, cx - s, CGRectGetMidY(bottom) - s / 2);
        CGContextAddLineToPoint(inContext, cx + s, CGRectGetMidY(bottom) - s / 2);
        CGContextAddLineToPoint(inContext, cx, CGRectGetMidY(bottom) + s / 2);
        CGContextClosePath(inContext);
        CGContextFillPath(inContext);
    } else {
        Boolean isDefault = (inDrawInfo->adornment & kThemeAdornmentDefault) != 0;
        Boolean header = k == kThemeListHeaderButton;
        CGFloat radius = header ? 0 : (b.size.height < 12 ? b.size.height / 2 : 5);
        Colour face = isDefault && !header ? kAccent : (pressed || on ? kFacePressed : kFace);
        fillAndFrame(inContext, b, radius, face, isDefault && !header ? kAccent : kFrame, alpha);
        if (isPopup(k)) {
            CGRect arrow = CGRectMake(CGRectGetMaxX(b) - 20, b.origin.y, 18, b.size.height);
            Colour t = kText;
            t.a = alpha;
            triangle(inContext, arrow, true, t);
        }
    }
    if (inDrawInfo->adornment & kThemeAdornmentFocus) {
        CGContextSetRGBStrokeColor(inContext, kAccent.r, kAccent.g, kAccent.b, 0.6);
        CGContextSetLineWidth(inContext, 2.0);
        roundedRect(inContext, CGRectInset(b, -1, -1), 6);
        CGContextStrokePath(inContext);
    }
    CGContextRestoreGState(inContext);
    if (outLabelRect != NULL)
        HIThemeGetButtonContentBounds(inBounds, inDrawInfo, outLabelRect);
    return noErr;
}

/* ---- Tracks ---- */

static Boolean isScrollBar(ThemeTrackKind k) {
    return k == kThemeScrollBarMedium || k == kThemeScrollBarSmall || k == kThemeScrollBarMini;
}

static Boolean isSlider(ThemeTrackKind k) {
    return k == kThemeSliderMedium || k == kThemeSliderSmall || k == kThemeSliderMini;
}

static Boolean isIndeterminate(ThemeTrackKind k) {
    return k == kThemeIndeterminateBarMedium || k == kThemeIndeterminateBarLarge || k == kThemeIndeterminateBarMini;
}

/* The fraction of the range the value is at. */
static CGFloat fraction(const HIThemeTrackDrawInfo *d) {
    CGFloat range = (CGFloat)d->max - (CGFloat)d->min;
    if (range <= 0)
        return 0;
    return fmin(1, fmax(0, ((CGFloat)d->value - (CGFloat)d->min) / range));
}

OSStatus HIThemeDrawTrack(const HIThemeTrackDrawInfo *inDrawInfo, const HIRect *inGhostRect,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    const HIThemeTrackDrawInfo *d = inDrawInfo;
    CGRect b = d->bounds;
    Boolean horizontal = (d->attributes & kThemeTrackHorizontal) != 0;
    Boolean enabled = d->enableState == kThemeTrackActive;
    CGFloat alpha = enabled ? 1.0 : 0.5;
    CGFloat f = fraction(d);

    CGContextSaveGState(inContext);
    if (isScrollBar(d->kind)) {
        fillAndFrame(inContext, b, 0, kTrack, kTrack, 1.0);
        if ((d->attributes & kThemeTrackShowThumb) && d->enableState != kThemeTrackNothingToScroll) {
            CGFloat length = horizontal ? b.size.width : b.size.height;
            CGFloat range = (CGFloat)d->max - (CGFloat)d->min;
            CGFloat view = d->trackInfo.scrollbar.viewsize;
            CGFloat thumb = range + view > 0 ? fmax(18, length * view / (range + view)) : length;
            thumb = fmin(thumb, length);
            CGFloat pos = (length - thumb) * f;
            CGRect t = horizontal ? CGRectMake(b.origin.x + pos, b.origin.y + 2, thumb, b.size.height - 4)
                                  : CGRectMake(b.origin.x + 2, b.origin.y + pos, b.size.width - 4, thumb);
            Colour k = d->trackInfo.scrollbar.pressState & kThemeThumbPressed ? (Colour){ 0.48, 0.48, 0.48, 1 } : kThumb;
            fillAndFrame(inContext, t, fmin(t.size.width, t.size.height) / 2, k, k, alpha);
        }
    } else if (isSlider(d->kind)) {
        CGFloat thick = 4;
        CGRect groove = horizontal ? CGRectMake(b.origin.x + 8, CGRectGetMidY(b) - thick / 2, b.size.width - 16, thick)
                                   : CGRectMake(CGRectGetMidX(b) - thick / 2, b.origin.y + 8, thick, b.size.height - 16);
        fillAndFrame(inContext, groove, thick / 2, kTrack, kFrame, alpha);
        CGFloat knob = 16;
        CGPoint centre = horizontal
            ? CGPointMake(groove.origin.x + groove.size.width * f, CGRectGetMidY(b))
            : CGPointMake(CGRectGetMidX(b), CGRectGetMaxY(groove) - groove.size.height * f);
        CGRect k = CGRectMake(centre.x - knob / 2, centre.y - knob / 2, knob, knob);
        fillAndFrame(inContext, k, knob / 2, d->trackInfo.slider.pressState ? kFacePressed : kWhite, kFrame, alpha);
    } else {
        /* A progress or relevance bar. */
        CGFloat radius = fmin(b.size.width, b.size.height) / 2;
        fillAndFrame(inContext, b, radius, kTrack, kFrame, alpha);
        CGRect done = b;
        if (isIndeterminate(d->kind)) {
            /* A block moving with the phase. */
            CGFloat w = (horizontal ? b.size.width : b.size.height) / 4;
            CGFloat travel = (horizontal ? b.size.width : b.size.height) - w;
            CGFloat pos = travel * ((d->trackInfo.progress.phase % 32) / 31.0);
            done = horizontal ? CGRectMake(b.origin.x + pos, b.origin.y, w, b.size.height)
                              : CGRectMake(b.origin.x, b.origin.y + pos, b.size.width, w);
        } else if (horizontal) {
            done.size.width *= f;
        } else {
            done.origin.y += done.size.height * (1 - f);
            done.size.height *= f;
        }
        if (done.size.width > 1 && done.size.height > 1)
            fillAndFrame(inContext, done, radius, kAccent, kAccent, alpha);
    }
    CGContextRestoreGState(inContext);
    return noErr;
}

/* ---- Tabs ---- */

OSStatus HIThemeDrawTab(const HIRect *inRect, const HIThemeTabDrawInfo *inDrawInfo, CGContextRef inContext,
    HIThemeOrientation inOrientation, HIRect *outLabelRect) {
    if (inRect == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    ThemeTabStyle s = inDrawInfo->style;
    Boolean front = s == kThemeTabFront || s == kThemeTabFrontInactive || s == kThemeTabFrontUnavailable;
    Boolean active = s != kThemeTabNonFrontInactive && s != kThemeTabFrontInactive &&
                     s != kThemeTabNonFrontUnavailable && s != kThemeTabFrontUnavailable;
    Colour face = front ? kWhite : (s == kThemeTabNonFrontPressed ? kFacePressed : kFace);
    CGContextSaveGState(inContext);
    fillAndFrame(inContext, *inRect, 4, face, kFrame, active ? 1.0 : 0.6);
    if (front) {
        setFill(inContext, kAccent);
        CGRect r = *inRect;
        CGContextFillRect(inContext, CGRectMake(r.origin.x + 4, CGRectGetMaxY(r) - 3, r.size.width - 8, 2));
    }
    CGContextRestoreGState(inContext);
    if (outLabelRect != NULL)
        *outLabelRect = CGRectInset(*inRect, 8, 2);
    return noErr;
}

OSStatus HIThemeDrawTabPane(const HIRect *inRect, const HIThemeTabPaneDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inRect == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    CGContextSaveGState(inContext);
    fillAndFrame(inContext, *inRect, 4, (Colour){ 0.95, 0.95, 0.95, 1 }, kFrame,
        stateActive(inDrawInfo->state) ? 1.0 : 0.6);
    CGContextRestoreGState(inContext);
    return noErr;
}

/* ---- Frames, group boxes, separators, the grow box, backgrounds ---- */

OSStatus HIThemeDrawFrame(const HIRect *inRect, const HIThemeFrameDrawInfo *inDrawInfo, CGContextRef inContext,
    HIThemeOrientation inOrientation) {
    if (inRect == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    CGContextSaveGState(inContext);
    setStroke(inContext, inDrawInfo->isFocused ? kAccent : kFrame);
    CGContextSetLineWidth(inContext, inDrawInfo->isFocused ? 2.0 : 1.0);
    CGContextStrokeRect(inContext, CGRectInset(*inRect, 0.5, 0.5));
    CGContextRestoreGState(inContext);
    return noErr;
}

OSStatus HIThemeDrawGroupBox(const HIRect *inRect, const HIThemeGroupBoxDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inRect == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    Boolean opaque = inDrawInfo->kind == kHIThemeGroupBoxKindPrimaryOpaque ||
                     inDrawInfo->kind == kHIThemeGroupBoxKindSecondaryOpaque;
    Colour face = opaque ? (Colour){ 0.90, 0.90, 0.90, 1 } : (Colour){ 0, 0, 0, 0.04 };
    CGContextSaveGState(inContext);
    fillAndFrame(inContext, *inRect, 5, face, (Colour){ 0.75, 0.75, 0.75, 1 }, 1.0);
    CGContextRestoreGState(inContext);
    return noErr;
}

OSStatus HIThemeDrawSeparator(const HIRect *inRect, const HIThemeSeparatorDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inRect == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    CGRect r = *inRect;
    CGContextSaveGState(inContext);
    setFill(inContext, (Colour){ 0.75, 0.75, 0.75, 1 });
    if (r.size.width >= r.size.height)
        CGContextFillRect(inContext, CGRectMake(r.origin.x, floor(CGRectGetMidY(r)), r.size.width, 1));
    else
        CGContextFillRect(inContext, CGRectMake(floor(CGRectGetMidX(r)), r.origin.y, 1, r.size.height));
    CGContextRestoreGState(inContext);
    return noErr;
}

OSStatus HIThemeGetGrowBoxBounds(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo, HIRect *outBounds) {
    if (inOrigin == NULL || inDrawInfo == NULL || outBounds == NULL)
        return paramErr;
    CGFloat size = inDrawInfo->size == kHIThemeGrowBoxSizeSmall ? 11 : 15;
    CGFloat x = (inDrawInfo->direction & kThemeGrowLeft) ? inOrigin->x : inOrigin->x - size;
    CGFloat y = (inDrawInfo->direction & kThemeGrowUp) ? inOrigin->y : inOrigin->y - size;
    *outBounds = CGRectMake(x, y, size, size);
    return noErr;
}

OSStatus HIThemeDrawGrowBox(const HIPoint *inOrigin, const HIThemeGrowBoxDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    CGRect b;
    if (inContext == NULL || HIThemeGetGrowBoxBounds(inOrigin, inDrawInfo, &b) != noErr)
        return paramErr;
    if (inDrawInfo->kind == kHIThemeGrowBoxKindNone)
        return noErr;
    CGContextSaveGState(inContext);
    setStroke(inContext, kFrame);
    CGContextSetLineWidth(inContext, 1.0);
    for (int i = 1; i <= 3; i++) {
        CGFloat d = b.size.width * i / 4;
        CGContextMoveToPoint(inContext, CGRectGetMaxX(b) - d, CGRectGetMaxY(b) - 1);
        CGContextAddLineToPoint(inContext, CGRectGetMaxX(b) - 1, CGRectGetMaxY(b) - d);
    }
    CGContextStrokePath(inContext);
    CGContextRestoreGState(inContext);
    return noErr;
}

OSStatus HIThemeApplyBackground(const HIRect *inBounds, const HIThemeBackgroundDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation) {
    if (inBounds == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    switch (inDrawInfo->kind) {
    case kThemeBackgroundTabPane: setFill(inContext, (Colour){ 0.95, 0.95, 0.95, 1 }); break;
    case kThemeBackgroundSecondaryGroupBox: setFill(inContext, (Colour){ 0.90, 0.90, 0.90, 1 }); break;
    case kThemeBackgroundWindowHeader:
    case kThemeBackgroundListViewWindowHeader: setFill(inContext, (Colour){ 0.94, 0.94, 0.94, 1 }); break;
    case kThemeBackgroundPlacard: setFill(inContext, kFace); break;
    default: setFill(inContext, kWindow); break;
    }
    return noErr;
}
