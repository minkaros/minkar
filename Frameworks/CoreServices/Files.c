/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An FSRef holds a tag, the volume (device) and the file number (inode),
 * and the path it was made from as a hint. Back to a path: the hint while
 * it still names that file, else the volume's file lookup by number.
 */

#include "Files.h"
#include "MacErrors.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsgetpath.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct RefData {
    UInt32 tag;
    UInt32 hintID;
    UInt64 device;
    UInt64 inode;
} RefData;

enum { kRefTag = 0x4d6b5246 }; /* 'MkRF' */

/* Path hints live in a process-wide table: an FSRef only holds the index. */
static char **sHints;
static UInt32 sHintCount;

static UInt32 rememberPath(const char *path) {
    for (UInt32 i = 0; i < sHintCount; i++)
        if (strcmp(sHints[i], path) == 0)
            return i + 1;
    char **grown = realloc(sHints, (sHintCount + 1) * sizeof(char *));
    if (grown == NULL)
        return 0;
    sHints = grown;
    sHints[sHintCount] = strdup(path);
    if (sHints[sHintCount] == NULL)
        return 0;
    return ++sHintCount;
}

static OSStatus errnoStatus(int e) {
    switch (e) {
    case ENOENT: return fnfErr;
    case ENOTDIR: return dirNFErr;
    case EACCES: case EPERM: return permErr;
    case ENAMETOOLONG: return bdNamErr;
    default: return ioErr;
    }
}

OSStatus FSPathMakeRefWithOptions(const UInt8 *path, OptionBits options, FSRef *ref, Boolean *isDirectory) {
    struct stat st;
    char absolute[PATH_MAX];
    RefData data;

    if (path == NULL || ref == NULL)
        return paramErr;
    if (((options & kFSPathMakeRefDoNotFollowLeafSymlink) ? lstat((const char *)path, &st) : stat((const char *)path, &st)) != 0)
        return errnoStatus(errno);
    if (realpath((const char *)path, absolute) == NULL || (options & kFSPathMakeRefDoNotFollowLeafSymlink))
        strlcpy(absolute, (const char *)path, sizeof(absolute));
    memset(ref, 0, sizeof(*ref));
    data.tag = kRefTag;
    data.hintID = rememberPath(absolute);
    data.device = (UInt64)st.st_dev;
    data.inode = (UInt64)st.st_ino;
    memcpy(ref->hidden, &data, sizeof(data));
    if (isDirectory != NULL)
        *isDirectory = S_ISDIR(st.st_mode);
    return noErr;
}

OSStatus FSPathMakeRef(const UInt8 *path, FSRef *ref, Boolean *isDirectory) {
    return FSPathMakeRefWithOptions(path, kFSPathMakeRefDefaultOptions, ref, isDirectory);
}

static Boolean readRef(const FSRef *ref, RefData *data) {
    if (ref == NULL)
        return false;
    memcpy(data, ref->hidden, sizeof(*data));
    return data->tag == kRefTag;
}

Boolean FSIsFSRefValid(const FSRef *ref) {
    RefData data;
    return readRef(ref, &data);
}

/* The path of the file by its number on its volume (fsgetpath). */
static Boolean pathByNumber(const RefData *data, char *out, size_t size) {
    struct statfs *mounts = NULL;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < count; i++) {
        struct stat st;
        if (stat(mounts[i].f_mntonname, &st) == 0 && (UInt64)st.st_dev == data->device) {
            fsid_t fsid = mounts[i].f_fsid;
            ssize_t n = fsgetpath(out, size, &fsid, data->inode);
            return n > 0;
        }
    }
    return false;
}

OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 pathBufferSize) {
    RefData data;
    char found[PATH_MAX];
    struct stat st;

    if (path == NULL || pathBufferSize == 0)
        return paramErr;
    if (!readRef(ref, &data))
        return paramErr;
    if (data.hintID > 0 && data.hintID <= sHintCount && lstat(sHints[data.hintID - 1], &st) == 0 &&
        (UInt64)st.st_dev == data.device && (UInt64)st.st_ino == data.inode)
        strlcpy(found, sHints[data.hintID - 1], sizeof(found));
    else if (!pathByNumber(&data, found, sizeof(found)))
        return fnfErr;
    if (strlen(found) + 1 > pathBufferSize)
        return bdNamErr;
    strlcpy((char *)path, found, pathBufferSize);
    return noErr;
}

OSErr FSCompareFSRefs(const FSRef *ref1, const FSRef *ref2) {
    RefData a, b;
    if (!readRef(ref1, &a) || !readRef(ref2, &b))
        return paramErr;
    if (a.device != b.device)
        return diffVolErr;
    return (a.inode == b.inode) ? noErr : errFSRefsDifferent;
}
