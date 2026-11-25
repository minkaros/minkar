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
#include <Carbon/HIShape.h>
#include <Carbon/HITheme.h>
#include <Carbon/MacWindows.h>

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

#ifdef __cplusplus
}
#endif

#endif
