/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * File system references (FSRef) made from paths and back. A reference
 * names an existing file by its volume and file number, so it follows the
 * file when it is renamed or moved on its volume.
 */

#ifndef __FILES__
#define __FILES__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FSRef {
    UInt8 hidden[80];
};
typedef struct FSRef FSRef;
typedef FSRef *FSRefPtr;

typedef SInt16 FSVolumeRefNum;
typedef SInt16 ResFileRefNum;
typedef SInt8 SignedByte;

enum {
    kFSPathMakeRefDefaultOptions = 0,
    kFSPathMakeRefDoNotFollowLeafSymlink = 0x01
};

enum {
    fsCurPerm = 0x00,
    fsRdPerm = 0x01,
    fsWrPerm = 0x02,
    fsRdWrPerm = 0x03,
    fsRdWrShPerm = 0x04
};

OSStatus FSPathMakeRef(const UInt8 *path, FSRef *ref, Boolean *isDirectory);
OSStatus FSPathMakeRefWithOptions(const UInt8 *path, OptionBits options, FSRef *ref, Boolean *isDirectory);
OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 pathBufferSize);
OSErr FSCompareFSRefs(const FSRef *ref1, const FSRef *ref2);
Boolean FSIsFSRefValid(const FSRef *ref);

#ifdef __cplusplus
}
#endif

#endif
