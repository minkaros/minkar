/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The C pasteboard API, over the app's NSPasteboard of the same name
 * (AppKit must be loaded). Only what the apps ask for so far: creating a
 * reference, synchronizing it, and the paste location a drop target
 * gives a drag's source for its promised files.
 */

#ifndef _APPLICATIONSERVICES_PASTEBOARD_H_
#define _APPLICATIONSERVICES_PASTEBOARD_H_

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef struct OpaquePasteboardRef *PasteboardRef;

#define kPasteboardClipboard CFSTR("com.apple.pasteboard.clipboard")
#define kPasteboardFind CFSTR("com.apple.pasteboard.find")
#define kPasteboardUniqueName ((CFStringRef)NULL)

typedef CF_OPTIONS(OptionBits, PasteboardSyncFlags) {
    kPasteboardModified = (1u << 0),
    kPasteboardClientIsOwner = (1u << 31)
};

enum {
    badPasteboardSyncErr = -25130,
    badPasteboardIndexErr = -25131,
    badPasteboardItemErr = -25132,
    badPasteboardFlavorErr = -25133,
    duplicatePasteboardFlavorErr = -25134,
    notPasteboardOwnerErr = -25135,
    noPasteboardPromiseKeeperErr = -25136
};

CFTypeID PasteboardGetTypeID(void);
OSStatus PasteboardCreate(CFStringRef _Nullable inName, PasteboardRef CF_RETURNS_RETAINED _Nullable *_Nonnull outPasteboard);
PasteboardSyncFlags PasteboardSynchronize(PasteboardRef inPasteboard);
OSStatus PasteboardCopyName(PasteboardRef inPasteboard, CFStringRef CF_RETURNS_RETAINED _Nullable *_Nonnull outName);
OSStatus PasteboardSetPasteLocation(PasteboardRef inPasteboard, CFURLRef inPasteLocation);
OSStatus PasteboardCopyPasteLocation(PasteboardRef inPasteboard, CFURLRef CF_RETURNS_RETAINED _Nullable *_Nonnull outPasteLocation);

CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
