/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * CFURL resource properties of file URLs, from stat(2):
 * CFURLResourceIsReachable, CFURLCopyResourcePropertyForKey and
 * CFURLCopyResourcePropertiesForKeys, with the file system keys that
 * stat answers. The keys have macOS's values, which are NSURL's.
 * A key that is not supported has no value (true, and NULL), as for a
 * property the resource does not have. Values are not cached.
 */

#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KEY(name, value)	const CFStringRef name = CFSTR(value)

KEY(kCFURLNameKey,				"NSURLNameKey");
KEY(kCFURLPathKey,				"_NSURLPathKey");
KEY(kCFURLIsRegularFileKey,			"NSURLIsRegularFileKey");
KEY(kCFURLIsDirectoryKey,			"NSURLIsDirectoryKey");
KEY(kCFURLIsSymbolicLinkKey,			"NSURLIsSymbolicLinkKey");
KEY(kCFURLIsVolumeKey,				"NSURLIsVolumeKey");
KEY(kCFURLIsHiddenKey,				"NSURLIsHiddenKey");
KEY(kCFURLIsReadableKey,			"NSURLIsReadableKey");
KEY(kCFURLIsWritableKey,			"NSURLIsWritableKey");
KEY(kCFURLIsExecutableKey,			"NSURLIsExecutableKey");
KEY(kCFURLIsSystemImmutableKey,			"NSURLIsSystemImmutableKey");
KEY(kCFURLIsUserImmutableKey,			"NSURLIsUserImmutableKey");
KEY(kCFURLCreationDateKey,			"NSURLCreationDateKey");
KEY(kCFURLContentAccessDateKey,			"NSURLContentAccessDateKey");
KEY(kCFURLContentModificationDateKey,		"NSURLContentModificationDateKey");
KEY(kCFURLAttributeModificationDateKey,		"NSURLAttributeModificationDateKey");
KEY(kCFURLLinkCountKey,				"NSURLLinkCountKey");
KEY(kCFURLParentDirectoryURLKey,		"NSURLParentDirectoryURLKey");
KEY(kCFURLFileResourceTypeKey,			"NSURLFileResourceTypeKey");
KEY(kCFURLFileSizeKey,				"NSURLFileSizeKey");
KEY(kCFURLFileAllocatedSizeKey,			"NSURLFileAllocatedSizeKey");
KEY(kCFURLTotalFileSizeKey,			"NSURLTotalFileSizeKey");
KEY(kCFURLTotalFileAllocatedSizeKey,		"NSURLTotalFileAllocatedSizeKey");

KEY(kCFURLFileResourceTypeNamedPipe,		"NSFileTypeNamedPipe");
KEY(kCFURLFileResourceTypeCharacterSpecial,	"NSFileTypeCharacterSpecial");
KEY(kCFURLFileResourceTypeDirectory,		"NSFileTypeDirectory");
KEY(kCFURLFileResourceTypeBlockSpecial,		"NSFileTypeBlockSpecial");
KEY(kCFURLFileResourceTypeRegular,		"NSFileTypeRegular");
KEY(kCFURLFileResourceTypeSymbolicLink,		"NSFileTypeSymbolicLink");
KEY(kCFURLFileResourceTypeSocket,		"NSFileTypeSocket");
KEY(kCFURLFileResourceTypeUnknown,		"NSFileTypeUnknown");

/* The URL's path, if it is a file URL. */
static Boolean
copyPath(CFURLRef url, char *path, size_t size, CFErrorRef *error)
{
    CFStringRef scheme = CFURLCopyScheme(url);
    Boolean isFile = scheme != NULL &&
        CFStringCompare(scheme, CFSTR("file"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;

    if (scheme != NULL)
        CFRelease(scheme);
    if (isFile && CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, (CFIndex)size))
        return true;
    if (error != NULL)
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainCocoa,
            isFile ? 4 /* NSFileNoSuchFileError */ : 262 /* NSFileReadUnsupportedSchemeError */, NULL);
    return false;
}

static void
setErrno(CFErrorRef *error, int err)
{
    if (error != NULL)
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX, err, NULL);
}

static CFDateRef
createDate(struct timespec ts)
{
    return CFDateCreate(kCFAllocatorDefault,
        (CFAbsoluteTime)ts.tv_sec - kCFAbsoluteTimeIntervalSince1970 + ts.tv_nsec / 1.0e9);
}

static CFNumberRef
createNumber(long long value)
{
    return CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &value);
}

static CFStringRef
resourceType(mode_t mode)
{
    switch (mode & S_IFMT) {
    case S_IFIFO:	return kCFURLFileResourceTypeNamedPipe;
    case S_IFCHR:	return kCFURLFileResourceTypeCharacterSpecial;
    case S_IFDIR:	return kCFURLFileResourceTypeDirectory;
    case S_IFBLK:	return kCFURLFileResourceTypeBlockSpecial;
    case S_IFREG:	return kCFURLFileResourceTypeRegular;
    case S_IFLNK:	return kCFURLFileResourceTypeSymbolicLink;
    case S_IFSOCK:	return kCFURLFileResourceTypeSocket;
    default:		return kCFURLFileResourceTypeUnknown;
    }
}

/* The value of key for the file at path (lstat'ed as st), retained; NULL if none. */
static CFTypeRef
copyValue(CFURLRef url, const char *path, const struct stat *st, CFStringRef key)
{
    if (CFEqual(key, kCFURLNameKey))
        return CFURLCopyLastPathComponent(url);
    if (CFEqual(key, kCFURLPathKey))
        return CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    if (CFEqual(key, kCFURLIsRegularFileKey))
        return CFRetain(S_ISREG(st->st_mode) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsDirectoryKey))
        return CFRetain(S_ISDIR(st->st_mode) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsSymbolicLinkKey))
        return CFRetain(S_ISLNK(st->st_mode) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsVolumeKey)) {
        struct stat parent;
        char up[PATH_MAX];

        snprintf(up, sizeof(up), "%s/..", path);
        return CFRetain(S_ISDIR(st->st_mode) && stat(up, &parent) == 0 &&
            (parent.st_dev != st->st_dev || parent.st_ino == st->st_ino) ?
            kCFBooleanTrue : kCFBooleanFalse);
    }
    if (CFEqual(key, kCFURLIsHiddenKey)) {
        CFStringRef name = CFURLCopyLastPathComponent(url);
        Boolean hidden = (st->st_flags & UF_HIDDEN) != 0 ||
            (name != NULL && CFStringHasPrefix(name, CFSTR(".")));

        if (name != NULL)
            CFRelease(name);
        return CFRetain(hidden ? kCFBooleanTrue : kCFBooleanFalse);
    }
    if (CFEqual(key, kCFURLIsReadableKey))
        return CFRetain(access(path, R_OK) == 0 ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsWritableKey))
        return CFRetain(access(path, W_OK) == 0 ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsExecutableKey))
        return CFRetain(access(path, X_OK) == 0 ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsSystemImmutableKey))
        return CFRetain((st->st_flags & SF_IMMUTABLE) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLIsUserImmutableKey))
        return CFRetain((st->st_flags & UF_IMMUTABLE) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(key, kCFURLCreationDateKey))
        return createDate(st->st_birthtimespec);
    if (CFEqual(key, kCFURLContentAccessDateKey))
        return createDate(st->st_atimespec);
    if (CFEqual(key, kCFURLContentModificationDateKey))
        return createDate(st->st_mtimespec);
    if (CFEqual(key, kCFURLAttributeModificationDateKey))
        return createDate(st->st_ctimespec);
    if (CFEqual(key, kCFURLLinkCountKey))
        return createNumber(st->st_nlink);
    if (CFEqual(key, kCFURLParentDirectoryURLKey))
        return CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, url);
    if (CFEqual(key, kCFURLFileResourceTypeKey))
        return CFRetain(resourceType(st->st_mode));
    /* Sizes are only for regular files, as on macOS. */
    if (CFEqual(key, kCFURLFileSizeKey) || CFEqual(key, kCFURLTotalFileSizeKey))
        return S_ISREG(st->st_mode) ? createNumber(st->st_size) : NULL;
    if (CFEqual(key, kCFURLFileAllocatedSizeKey) || CFEqual(key, kCFURLTotalFileAllocatedSizeKey))
        return S_ISREG(st->st_mode) ? createNumber((long long)st->st_blocks * 512) : NULL;
    return NULL;
}

Boolean
CFURLResourceIsReachable(CFURLRef url, CFErrorRef *error)
{
    char path[PATH_MAX];
    struct stat st;

    if (error != NULL)
        *error = NULL;
    if (!copyPath(url, path, sizeof(path), error))
        return false;
    if (lstat(path, &st) != 0) {
        setErrno(error, errno);
        return false;
    }
    return true;
}

Boolean
CFURLCopyResourcePropertyForKey(CFURLRef url, CFStringRef key, void *propertyValueTypeRefPtr,
    CFErrorRef *error)
{
    char path[PATH_MAX];
    struct stat st;

    if (error != NULL)
        *error = NULL;
    if (propertyValueTypeRefPtr != NULL)
        *(CFTypeRef *)propertyValueTypeRefPtr = NULL;
    if (!copyPath(url, path, sizeof(path), error))
        return false;
    if (lstat(path, &st) != 0) {
        setErrno(error, errno);
        return false;
    }
    if (propertyValueTypeRefPtr != NULL)
        *(CFTypeRef *)propertyValueTypeRefPtr = copyValue(url, path, &st, key);
    return true;
}

CFDictionaryRef
CFURLCopyResourcePropertiesForKeys(CFURLRef url, CFArrayRef keys, CFErrorRef *error)
{
    char path[PATH_MAX];
    struct stat st;
    CFMutableDictionaryRef dict;

    if (error != NULL)
        *error = NULL;
    if (!copyPath(url, path, sizeof(path), error))
        return NULL;
    if (lstat(path, &st) != 0) {
        setErrno(error, errno);
        return NULL;
    }
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (CFIndex i = 0, n = keys != NULL ? CFArrayGetCount(keys) : 0; i < n; i++) {
        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
        CFTypeRef value = copyValue(url, path, &st, key);

        if (value != NULL) {
            CFDictionarySetValue(dict, key, value);
            CFRelease(value);
        }
    }
    return dict;
}
