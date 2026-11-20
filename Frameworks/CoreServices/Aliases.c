// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/Aliases.h>
#include <CoreServices/MacErrors.h>
#include <limits.h>

/* CoreFoundation's reader of alias records (private). */
extern CFStringRef _CFURLCopyPathFromAliasRecord(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length);

/* The record's size, from its header (bytes 4-5), within its handle. */
Size GetAliasSize(AliasHandle alias)
{
    const UInt8 *bytes;
    Size handleSize;

    if (alias == NULL || *alias == NULL)
        return 0;
    handleSize = GetHandleSize((Handle)alias);
    bytes = (const UInt8 *)*alias;
    if (handleSize < 8)
        return handleSize;
    Size size = (Size)((bytes[4] << 8) | bytes[5]);
    return (size > 0 && size <= handleSize) ? size : handleSize;
}

OSType GetAliasUserType(AliasHandle alias)
{
    const UInt8 *bytes;

    if (alias == NULL || *alias == NULL || GetHandleSize((Handle)alias) < 4)
        return 0;
    bytes = (const UInt8 *)*alias;
    return ((OSType)bytes[0] << 24) | ((OSType)bytes[1] << 16) | ((OSType)bytes[2] << 8) | bytes[3];
}

/* The file the record points to, by its path: an error when the record
   cannot be read (paramErr) or its file is not there (fnfErr). Nothing is
   mounted (mountFlags), and the record is never updated. */
OSErr FSResolveAliasWithMountFlags(const FSRef *fromFile, AliasHandle inAlias, FSRef *target, Boolean *wasChanged, unsigned long mountFlags)
{
    CFStringRef path;
    char buffer[PATH_MAX];
    OSStatus status;

    (void)fromFile;
    (void)mountFlags;
    if (wasChanged != NULL)
        *wasChanged = false;
    if (inAlias == NULL || *inAlias == NULL || target == NULL)
        return paramErr;
    path = _CFURLCopyPathFromAliasRecord(kCFAllocatorDefault, (const UInt8 *)*inAlias, GetAliasSize(inAlias));
    if (path == NULL)
        return paramErr;
    if (!CFStringGetFileSystemRepresentation(path, buffer, sizeof(buffer))) {
        CFRelease(path);
        return paramErr;
    }
    CFRelease(path);
    status = FSPathMakeRef((const UInt8 *)buffer, target, NULL);
    return (status == noErr) ? noErr : fnfErr;
}

OSErr FSResolveAlias(const FSRef *fromFile, AliasHandle alias, FSRef *target, Boolean *wasChanged)
{
    return FSResolveAliasWithMountFlags(fromFile, alias, target, wasChanged, 0);
}
