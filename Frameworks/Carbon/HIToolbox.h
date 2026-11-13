/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __HITOOLBOX__
#define __HITOOLBOX__

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbolic hot keys (the system's own shortcuts) disabled while a mode
   is pushed, so an application can take the keys. Minkar's shortcuts
   are the window server's and the menu bar's: the modes are counted and
   handed out; nothing is disabled yet. */
enum {
    kHIHotKeyModeAllEnabled = 0,
    kHIHotKeyModeAllDisabled = (1 << 0),
    kHIHotKeyModeAllDisabledExceptUniversalAccess = (1 << 1)
};

void *PushSymbolicHotKeyMode(OptionBits inOptions);
void PopSymbolicHotKeyMode(void *inToken);
OptionBits GetSymbolicHotKeyMode(void);

/* The modifier keys held now, as Carbon's event modifiers. */
enum {
    activeFlag = 0x0001,
    btnState = 0x0080,
    cmdKey = 0x0100,
    shiftKey = 0x0200,
    alphaLock = 0x0400,
    optionKey = 0x0800,
    controlKey = 0x1000,
    rightShiftKey = 0x2000,
    rightOptionKey = 0x4000,
    rightControlKey = 0x8000
};
UInt32 GetCurrentEventKeyModifiers(void);
UInt32 GetCurrentKeyModifiers(void);

/* Secure event input: while on, keystrokes are not given to event taps
   of other processes. Counted per process. */
OSStatus EnableSecureEventInput(void);
OSStatus DisableSecureEventInput(void);
Boolean IsSecureEventInputEnabled(void);

/* The physical keyboard: Minkar reports an ANSI one. */
typedef UInt32 PhysicalKeyboardLayoutType;
enum {
    kKeyboardJIS = 'JIS ',
    kKeyboardANSI = 'ANSI',
    kKeyboardISO = 'ISO ',
    kKeyboardUnknown = kUnknownType
};
PhysicalKeyboardLayoutType KBGetLayoutType(SInt16 iKeyboardType);
UInt8 LMGetKbdType(void);

/* Text services documents: an application's text input context and its
   properties. */
typedef struct OpaqueTSMDocumentID *TSMDocumentID;
typedef OSType TSMDocumentPropertyTag;
enum {
    tsmInvalidDocumentErr = -2505,
    tsmDocPropertyNotFoundErr = -2528,
    tsmDocPropertyBufferTooSmallErr = -2529
};
TSMDocumentID TSMGetActiveDocument(void);
OSStatus TSMSetDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag,
    UInt32 propertySize, void *propertyData);
OSStatus TSMGetDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag,
    UInt32 bufferSize, UInt32 *actualSize, void *propertyBuffer);
OSStatus TSMRemoveDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag);

/* Appearance metrics, in points: Atlas's. */
typedef UInt32 ThemeMetric;
enum {
    kThemeMetricScrollBarWidth = 0,
    kThemeMetricSmallScrollBarWidth = 1,
    kThemeMetricCheckBoxHeight = 2,
    kThemeMetricRadioButtonHeight = 3,
    kThemeMetricEditTextWhitespace = 4,
    kThemeMetricEditTextFrameOutset = 5,
    kThemeMetricListBoxFrameOutset = 6,
    kThemeMetricFocusRectOutset = 7
};
enum { themeInvalidMetricErr = -30570 };
OSStatus GetThemeMetric(ThemeMetric inMetric, SInt32 *outMetric);

/* A button drawn in Atlas's look. */
typedef UInt32 ThemeDrawState;
typedef UInt16 ThemeButtonKind;
typedef UInt16 ThemeButtonValue;
typedef UInt16 ThemeButtonAdornment;
typedef UInt32 HIThemeOrientation;
enum {
    kThemeStateInactive = 0,
    kThemeStateActive = 1,
    kThemeStatePressed = 2,
    kThemeStateDisabled = 0
};
enum {
    kThemeButtonOff = 0,
    kThemeButtonOn = 1,
    kThemeButtonMixed = 2
};
enum {
    kHIThemeOrientationNormal = 0,
    kHIThemeOrientationInverted = 1
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

OSStatus HIThemeDrawButton(const CGRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation, CGRect *outLabelRect);

#ifdef __cplusplus
}
#endif

#endif
