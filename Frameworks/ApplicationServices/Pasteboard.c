/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The C pasteboard API (Pasteboard.h). A PasteboardRef names an
 * NSPasteboard and keeps the change count it last saw; AppKit is reached
 * through the Objective-C runtime, as it loads after this framework.
 *
 * The paste location: a drop target that takes a drag's promised files
 * says where it wants them with PasteboardSetPasteLocation; the source
 * reads it with PasteboardCopyPasteLocation and writes the files there
 * (Firefox does so when an image or a link is dragged to a folder). It
 * travels on the drag pasteboard as a type of Minkar's own.
 */

#include "Pasteboard.h"
#include <CoreServices/MacErrors.h>
#include <CoreFoundation/CFRuntime.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

#define kPasteLocationType CFSTR("org.minkar.pasteboard.paste-location")

struct OpaquePasteboardRef {
    CFRuntimeBase base;
    CFStringRef name;           /* the NSPasteboard's */
    long changeCount;           /* as last synchronized; -1 never */
};

static CFTypeID sPasteboardTypeID;
static pthread_once_t sPasteboardOnce = PTHREAD_ONCE_INIT;

static void pasteboardFinalize(CFTypeRef cf) {
    struct OpaquePasteboardRef *p = (struct OpaquePasteboardRef *)cf;
    if (p->name != NULL)
        CFRelease(p->name);
}

static void registerPasteboard(void) {
    static CFRuntimeClass cls = { 0, "Pasteboard", NULL, NULL, pasteboardFinalize, NULL, NULL, NULL, NULL };
    sPasteboardTypeID = _CFRuntimeRegisterClass(&cls);
}

CFTypeID PasteboardGetTypeID(void) {
    pthread_once(&sPasteboardOnce, registerPasteboard);
    return sPasteboardTypeID;
}

/* AppKit's name for one of the C API's well-known pasteboards. */
static CFStringRef appKitName(CFStringRef name) {
    const char *symbol = NULL;
    if (CFEqual(name, kPasteboardClipboard))
        symbol = "NSPasteboardNameGeneral";
    else if (CFEqual(name, kPasteboardFind))
        symbol = "NSPasteboardNameFind";
    if (symbol != NULL) {
        CFStringRef *constant = dlsym(RTLD_DEFAULT, symbol);
        if (constant != NULL && *constant != NULL)
            return *constant;
    }
    return name;
}

static id nsPasteboard(struct OpaquePasteboardRef *p) {
    Class cls = objc_getClass("NSPasteboard");
    if (cls == Nil || p == NULL)
        return nil;
    return ((id (*)(id, SEL, id))objc_msgSend)((id)cls, sel_registerName("pasteboardWithName:"), (id)p->name);
}

OSStatus PasteboardCreate(CFStringRef inName, PasteboardRef *outPasteboard) {
    struct OpaquePasteboardRef *p;
    if (outPasteboard == NULL)
        return paramErr;
    *outPasteboard = NULL;
    p = (struct OpaquePasteboardRef *)_CFRuntimeCreateInstance(NULL, PasteboardGetTypeID(),
        sizeof(*p) - sizeof(CFRuntimeBase), NULL);
    if (p == NULL)
        return memFullErr;
    if (inName != NULL)
        p->name = CFRetain(appKitName(inName));
    else {
        /* A new pasteboard of its own. */
        CFUUIDRef uuid = CFUUIDCreate(NULL);
        CFStringRef text = CFUUIDCreateString(NULL, uuid);
        p->name = CFStringCreateWithFormat(NULL, NULL, CFSTR("org.minkar.pasteboard.unique.%@"), text);
        CFRelease(text);
        CFRelease(uuid);
    }
    p->changeCount = -1;
    *outPasteboard = p;
    return noErr;
}

PasteboardSyncFlags PasteboardSynchronize(PasteboardRef inPasteboard) {
    struct OpaquePasteboardRef *p = (struct OpaquePasteboardRef *)inPasteboard;
    id board = nsPasteboard(p);
    if (board == nil)
        return 0;
    long count = ((long (*)(id, SEL))objc_msgSend)(board, sel_registerName("changeCount"));
    PasteboardSyncFlags flags = (p->changeCount != count) ? kPasteboardModified : 0;
    p->changeCount = count;
    return flags;
}

OSStatus PasteboardCopyName(PasteboardRef inPasteboard, CFStringRef *outName) {
    struct OpaquePasteboardRef *p = (struct OpaquePasteboardRef *)inPasteboard;
    if (p == NULL || outName == NULL)
        return paramErr;
    *outName = CFRetain(p->name);
    return noErr;
}

OSStatus PasteboardSetPasteLocation(PasteboardRef inPasteboard, CFURLRef inPasteLocation) {
    id board = nsPasteboard((struct OpaquePasteboardRef *)inPasteboard);
    if (board == nil || inPasteLocation == NULL)
        return paramErr;
    CFArrayRef types = CFArrayCreate(NULL, (const void **)&(CFStringRef){ kPasteLocationType }, 1, &kCFTypeArrayCallBacks);
    ((long (*)(id, SEL, id, id))objc_msgSend)(board, sel_registerName("addTypes:owner:"), (id)types, nil);
    CFRelease(types);
    BOOL ok = ((BOOL (*)(id, SEL, id, id))objc_msgSend)(board, sel_registerName("setString:forType:"),
        (id)CFURLGetString(inPasteLocation), (id)kPasteLocationType);
    return ok ? noErr : badPasteboardSyncErr;
}

OSStatus PasteboardCopyPasteLocation(PasteboardRef inPasteboard, CFURLRef *outPasteLocation) {
    id board = nsPasteboard((struct OpaquePasteboardRef *)inPasteboard);
    if (outPasteLocation == NULL)
        return paramErr;
    *outPasteLocation = NULL;
    if (board == nil)
        return paramErr;
    CFStringRef text = (CFStringRef)((id (*)(id, SEL, id))objc_msgSend)(board, sel_registerName("stringForType:"),
        (id)kPasteLocationType);
    if (text == NULL)
        return badPasteboardFlavorErr;      /* no target gave one */
    *outPasteLocation = CFURLCreateWithString(NULL, text, NULL);
    return *outPasteLocation != NULL ? noErr : badPasteboardFlavorErr;
}
