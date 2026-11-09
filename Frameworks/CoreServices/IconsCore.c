/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "IconsCore.h"
#include "MacErrors.h"
#include <stdatomic.h>
#include <stdlib.h>

struct OpaqueIconRef {
    UInt32 magic;
    _Atomic UInt16 owners;
    OSType creator;
    OSType type;
    CFStringRef extension;
};

enum { kIconMagic = 0x49636f6e }; /* 'Icon' */

static IconRef makeIcon(OSType creator, OSType type, CFStringRef extension) {
    IconRef icon = calloc(1, sizeof(struct OpaqueIconRef));
    if (icon == NULL)
        return NULL;
    icon->magic = kIconMagic;
    icon->owners = 1;
    icon->creator = creator;
    icon->type = type;
    icon->extension = extension != NULL ? CFStringCreateCopy(NULL, extension) : NULL;
    return icon;
}

Boolean IsValidIconRef(IconRef theIconRef) {
    return theIconRef != NULL && theIconRef->magic == kIconMagic && theIconRef->owners > 0;
}

OSErr GetIconRef(SInt16 vRefNum, OSType creator, OSType iconType, IconRef *theIconRef) {
    if (theIconRef == NULL)
        return paramErr;
    *theIconRef = makeIcon(creator, iconType, NULL);
    return *theIconRef != NULL ? noErr : memFullErr;
}

OSErr GetIconRefFromTypeInfo(OSType inCreator, OSType inType, CFStringRef inExtension, CFStringRef inMIMEType,
                             IconServicesUsageFlags inUsageFlags, IconRef *outIconRef) {
    if (outIconRef == NULL)
        return paramErr;
    *outIconRef = makeIcon(inCreator, inType != 0 ? inType : kGenericDocumentIcon, inExtension);
    return *outIconRef != NULL ? noErr : memFullErr;
}

OSErr AcquireIconRef(IconRef theIconRef) {
    if (!IsValidIconRef(theIconRef))
        return paramErr;
    theIconRef->owners++;
    return noErr;
}

OSErr ReleaseIconRef(IconRef theIconRef) {
    if (!IsValidIconRef(theIconRef))
        return paramErr;
    if (--theIconRef->owners == 0) {
        theIconRef->magic = 0;
        if (theIconRef->extension != NULL)
            CFRelease(theIconRef->extension);
        free(theIconRef);
    }
    return noErr;
}

OSErr GetIconRefOwners(IconRef theIconRef, UInt16 *owners) {
    if (!IsValidIconRef(theIconRef) || owners == NULL)
        return paramErr;
    *owners = theIconRef->owners;
    return noErr;
}

OSType _IconRefGetType(IconRef theIconRef) {
    return IsValidIconRef(theIconRef) ? theIconRef->type : 0;
}

CFStringRef _IconRefGetExtension(IconRef theIconRef) {
    return IsValidIconRef(theIconRef) ? theIconRef->extension : NULL;
}
