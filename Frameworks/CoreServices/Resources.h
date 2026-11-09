/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Reading resources from a file's resource fork, or from a data fork in
 * resource format (FSOpenResourceFile with an empty fork name). Files are
 * opened for reading only; writing resources is not supported.
 */

#ifndef __RESOURCES__
#define __RESOURCES__

#include <CoreServices/Files.h>
#include <CoreServices/MacMemory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef FourCharCode ResType;
typedef SInt16 ResID;
typedef SInt16 ResAttributes;
typedef SInt16 ResourceIndex;
typedef SInt16 ResourceCount;

enum {
    resSysHeap = 64,
    resPurgeable = 32,
    resLocked = 16,
    resProtected = 8,
    resPreload = 4,
    resChanged = 2
};

enum { kResFileNotOpened = -1 };

OSErr ResError(void);
ResFileRefNum FSOpenResFile(const FSRef *ref, SInt8 permission);
OSErr FSOpenResourceFile(const FSRef *ref, UniCharCount forkNameLength, const UniChar *forkName, SInt8 permissions,
                         ResFileRefNum *refNum);
void CloseResFile(ResFileRefNum refNum);
ResFileRefNum CurResFile(void);
void UseResFile(ResFileRefNum refNum);

Handle Get1Resource(ResType theType, ResID theID);
Handle GetResource(ResType theType, ResID theID);
Handle Get1NamedResource(ResType theType, ConstStr255Param name);
Handle GetNamedResource(ResType theType, ConstStr255Param name);
Handle Get1IndResource(ResType theType, ResourceIndex index);
ResourceCount Count1Resources(ResType theType);
ResourceCount Count1Types(void);
void Get1IndType(ResType *theType, ResourceIndex index);
void GetResInfo(Handle theResource, ResID *theID, ResType *theType, Str255 name);
ResAttributes GetResAttrs(Handle theResource);
long GetResourceSizeOnDisk(Handle theResource);
void ReleaseResource(Handle theResource);
void DetachResource(Handle theResource);
void LoadResource(Handle theResource);
void SetResLoad(Boolean load);

#ifdef __cplusplus
}
#endif

#endif
