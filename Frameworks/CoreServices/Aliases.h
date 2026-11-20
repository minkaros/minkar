// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#ifndef __ALIASES__
#define __ALIASES__

#include <CoreServices/MacMemory.h>
#include <CoreServices/Files.h>

#ifdef __cplusplus
extern "C" {
#endif

/* An alias record (the classic Alias Manager's): where a file was, kept
   to find it again. Its contents are private; it is read here to resolve
   records other systems made. */
typedef struct AliasRecord {
    UInt8 hidden[6];
} AliasRecord;
typedef AliasRecord *AliasPtr;
typedef AliasPtr *AliasHandle;

enum {
    kResolveAliasFileNoUI = 0x00000001,
    kResolveAliasTryFileIDFirst = 0x00000002
};

OSErr FSResolveAlias(const FSRef *fromFile, AliasHandle alias, FSRef *target, Boolean *wasChanged);
OSErr FSResolveAliasWithMountFlags(const FSRef *fromFile, AliasHandle inAlias, FSRef *target, Boolean *wasChanged, unsigned long mountFlags);
Size GetAliasSize(AliasHandle alias);
OSType GetAliasUserType(AliasHandle alias);

#ifdef __cplusplus
}
#endif

#endif
