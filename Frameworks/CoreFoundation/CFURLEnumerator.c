/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFURLEnumerator: a directory's contents, or the mounted volumes, one URL
 * at a time. Depth first, with a stack of open directories; the URL
 * returned belongs to the enumerator until the next call. An error (a
 * directory that cannot be read) is returned once, and the enumeration
 * goes on with what is left. Written anew (13 Nov 2026) in place of an
 * earlier one that read a directory it had not opened.
 */

#include "CFURLEnumerator.h"
#include "CFInternal.h"
#include <CoreFoundation/CFRuntime.h>
#include <dirent.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>

typedef struct {
    DIR *dir;
    char *path;			/* the directory's path, malloc'd */
    CFURLRef url;		/* the directory's URL, for post order */
} Level;

struct __CFURLEnumerator {
    CFRuntimeBase _base;
    CFURLRef _root;
    char *_rootPath;
    CFURLEnumeratorOptions _options;
    Level *_levels;
    CFIndex _depth;		/* levels open */
    CFIndex _capacity;
    CFURLRef _current;		/* what was last returned */
    Boolean _currentIsDirectory;
    Boolean _skipCurrent;	/* SkipDescendents for _current */
    Boolean _pendingDescent;
    char *_pendingPath;		/* the directory to open next */
    CFURLRef _pendingURL;
    CFArrayRef _volumes;	/* for the mounted volumes: their URLs */
    CFIndex _volumeIndex;
};

static CFTypeID enumeratorTypeID = _kCFRuntimeNotATypeID;

static void
popLevel(struct __CFURLEnumerator *e)
{
    Level *l = &e->_levels[--e->_depth];
    if (l->dir != NULL)
        closedir(l->dir);
    free(l->path);
    if (l->url != NULL)
        CFRelease(l->url);
}

static void
enumeratorFinalize(CFTypeRef cf)
{
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)cf;
    while (e->_depth > 0)
        popLevel(e);
    free(e->_levels);
    free(e->_rootPath);
    free(e->_pendingPath);
    if (e->_pendingURL != NULL)
        CFRelease(e->_pendingURL);
    if (e->_root != NULL)
        CFRelease(e->_root);
    if (e->_current != NULL)
        CFRelease(e->_current);
    if (e->_volumes != NULL)
        CFRelease(e->_volumes);
}

static CFStringRef
enumeratorDescription(CFTypeRef cf)
{
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<CFURLEnumerator %p>{root = %@, level = %ld}"),
        cf, e->_root, (long)e->_depth);
}

static const CFRuntimeClass enumeratorClass = {
    0, "CFURLEnumerator", NULL, NULL, enumeratorFinalize, NULL, NULL, NULL, enumeratorDescription
};

CFTypeID
CFURLEnumeratorGetTypeID(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ enumeratorTypeID = _CFRuntimeRegisterClass(&enumeratorClass); });
    return enumeratorTypeID;
}

static struct __CFURLEnumerator *
createEnumerator(CFAllocatorRef alloc, CFURLEnumeratorOptions options)
{
    CFIndex size = sizeof(struct __CFURLEnumerator) - sizeof(CFRuntimeBase);
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)_CFRuntimeCreateInstance(alloc,
        CFURLEnumeratorGetTypeID(), size, NULL);
    if (e == NULL)
        return NULL;
    memset((char *)e + sizeof(CFRuntimeBase), 0, size);
    e->_options = options;
    return e;
}

static Boolean
pushLevel(struct __CFURLEnumerator *e, DIR *dir, const char *path, CFURLRef url)
{
    if (e->_depth == e->_capacity) {
        CFIndex capacity = e->_capacity ? e->_capacity * 2 : 8;
        Level *grown = realloc(e->_levels, capacity * sizeof(Level));
        if (grown == NULL)
            return false;
        e->_levels = grown;
        e->_capacity = capacity;
    }
    e->_levels[e->_depth].dir = dir;
    e->_levels[e->_depth].path = strdup(path);
    e->_levels[e->_depth].url = url != NULL ? CFRetain(url) : NULL;
    e->_depth++;
    return true;
}

static void
setError(CFErrorRef *error, int err, const char *path)
{
    if (error == NULL)
        return;
    CFStringRef p = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    const void *keys[] = { kCFErrorFilePathKey };
    const void *values[] = { p };
    *error = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, kCFErrorDomainPOSIX, err,
        keys, values, p != NULL ? 1 : 0);
    if (p != NULL)
        CFRelease(p);
}

CFURLEnumeratorRef
CFURLEnumeratorCreateForDirectoryURL(CFAllocatorRef alloc, CFURLRef directoryURL,
                                     CFURLEnumeratorOptions option, CFArrayRef propertyKeys)
{
    char path[PATH_MAX];
    if (directoryURL == NULL ||
        !CFURLGetFileSystemRepresentation(directoryURL, true, (UInt8 *)path, sizeof(path)))
        return NULL;
    struct __CFURLEnumerator *e = createEnumerator(alloc, option);
    if (e == NULL)
        return NULL;
    e->_root = CFRetain(directoryURL);
    e->_rootPath = strdup(path);
    /* The directory is opened by the first call, which reports an error. */
    e->_pendingDescent = true;
    e->_pendingPath = strdup(path);
    return (CFURLEnumeratorRef)e;
}

CFURLEnumeratorRef
CFURLEnumeratorCreateForMountedVolumes(CFAllocatorRef alloc, CFURLEnumeratorOptions option,
                                       CFArrayRef propertyKeys)
{
    struct __CFURLEnumerator *e = createEnumerator(alloc, option & kCFURLEnumeratorSkipInvisibles);
    if (e == NULL)
        return NULL;
    CFMutableArrayRef volumes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    int count = getfsstat(NULL, 0, MNT_NOWAIT);
    struct statfs *fs = count > 0 ? calloc(count, sizeof(struct statfs)) : NULL;
    if (fs != NULL)
        count = getfsstat(fs, count * (int)sizeof(struct statfs), MNT_NOWAIT);
    for (int i = 0; fs != NULL && i < count; i++) {
        const char *on = fs[i].f_mntonname;
        /* The volumes a user sees: the root and what is mounted under /Volumes. */
        Boolean visible = strcmp(on, "/") == 0 || strncmp(on, "/Volumes/", 9) == 0;
        if (!visible && (option & kCFURLEnumeratorSkipInvisibles))
            continue;
        if ((fs[i].f_flags & MNT_DONTBROWSE) && (option & kCFURLEnumeratorSkipInvisibles))
            continue;
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)on, strlen(on), true);
        if (url != NULL) {
            CFArrayAppendValue(volumes, url);
            CFRelease(url);
        }
    }
    free(fs);
    e->_volumes = volumes;
    return (CFURLEnumeratorRef)e;
}

static Boolean
isPackageName(const char *name)
{
    static const char *const extensions[] = { "app", "bundle", "framework", "plugin", "kext", "appex",
        "xpc", "rtfd", "pkg", "mpkg", "prefPane", "saver", "qlgenerator", "mdimporter", "component", NULL };
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return false;
    for (int i = 0; extensions[i] != NULL; i++)
        if (strcasecmp(dot + 1, extensions[i]) == 0)
            return true;
    return false;
}

static void
setCurrent(struct __CFURLEnumerator *e, CFURLRef url, Boolean isDirectory)
{
    if (e->_current != NULL)
        CFRelease(e->_current);
    e->_current = url;
    e->_currentIsDirectory = isDirectory;
    e->_skipCurrent = false;
}

/* The URL of path for the caller: relative to the root if asked. */
static CFURLRef
createURL(struct __CFURLEnumerator *e, const char *path, Boolean isDirectory)
{
    if ((e->_options & kCFURLEnumeratorGenerateRelativePathURLs) && e->_rootPath != NULL) {
        size_t n = strlen(e->_rootPath);
        const char *rest = path + n;
        if (strncmp(path, e->_rootPath, n) == 0 && *rest == '/') {
            rest++;
            return CFURLCreateFromFileSystemRepresentationRelativeToBase(kCFAllocatorDefault,
                (const UInt8 *)rest, strlen(rest), isDirectory, e->_root);
        }
    }
    return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)path, strlen(path), isDirectory);
}

CFURLEnumeratorResult
CFURLEnumeratorGetNextURL(CFURLEnumeratorRef enumerator, CFURLRef *url, CFErrorRef *error)
{
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)enumerator;
    if (url != NULL)
        *url = NULL;
    if (error != NULL)
        *error = NULL;
    if (e == NULL)
        return kCFURLEnumeratorError;

    if (e->_volumes != NULL) {
        if (e->_volumeIndex >= CFArrayGetCount(e->_volumes))
            return kCFURLEnumeratorEnd;
        CFURLRef volume = CFArrayGetValueAtIndex(e->_volumes, e->_volumeIndex++);
        setCurrent(e, CFRetain(volume), true);
        if (url != NULL)
            *url = volume;
        return kCFURLEnumeratorSuccess;
    }

    /* Into the directory last returned, unless told not to. */
    if (e->_current != NULL && e->_currentIsDirectory && !e->_skipCurrent &&
        (e->_options & kCFURLEnumeratorDescendRecursively) && !e->_pendingDescent) {
        char path[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(e->_current, true, (UInt8 *)path, sizeof(path))) {
            free(e->_pendingPath);
            e->_pendingPath = strdup(path);
            if (e->_pendingURL != NULL)
                CFRelease(e->_pendingURL);
            e->_pendingURL = CFRetain(e->_current);
            e->_pendingDescent = true;
        }
    }
    if (e->_pendingDescent) {
        e->_pendingDescent = false;
        DIR *dir = opendir(e->_pendingPath);
        CFURLRef dirURL = e->_pendingURL;
        e->_pendingURL = NULL;
        if (dir == NULL) {
            setError(error, errno, e->_pendingPath);
            if (dirURL != NULL)
                CFRelease(dirURL);
            return kCFURLEnumeratorError;
        }
        Boolean pushed = pushLevel(e, dir, e->_pendingPath, dirURL);
        if (dirURL != NULL)
            CFRelease(dirURL);
        if (!pushed) {
            closedir(dir);
            setError(error, ENOMEM, e->_pendingPath);
            return kCFURLEnumeratorError;
        }
    }

    while (e->_depth > 0) {
        Level *l = &e->_levels[e->_depth - 1];
        errno = 0;
        struct dirent *d = readdir(l->dir);
        if (d == NULL) {
            int err = errno;
            CFURLRef finished = l->url != NULL ? CFRetain(l->url) : NULL;
            char failedPath[PATH_MAX];
            strlcpy(failedPath, l->path, sizeof(failedPath));
            popLevel(e);
            if (err != 0) {
                if (finished != NULL)
                    CFRelease(finished);
                setError(error, err, failedPath);
                return kCFURLEnumeratorError;
            }
            /* A directory is given again after its contents when asked. */
            if (finished != NULL && (e->_options & kCFURLEnumeratorIncludeDirectoriesPostOrder)) {
                setCurrent(e, finished, false);
                if (url != NULL)
                    *url = finished;
                return kCFURLEnumeratorDirectoryPostOrderSuccess;
            }
            if (finished != NULL)
                CFRelease(finished);
            continue;
        }
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%s%s", l->path,
            l->path[0] != '\0' && l->path[strlen(l->path) - 1] == '/' ? "" : "/", d->d_name);
        struct stat st;
        Boolean isDirectory;
        if (d->d_type == DT_UNKNOWN || d->d_type == DT_LNK) {
            isDirectory = lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
        } else {
            isDirectory = d->d_type == DT_DIR;
        }
        if (e->_options & kCFURLEnumeratorSkipInvisibles) {
            if (d->d_name[0] == '.')
                continue;
            if (lstat(path, &st) == 0 && (st.st_flags & UF_HIDDEN))
                continue;
        }
        CFURLRef found = createURL(e, path, isDirectory);
        if (found == NULL)
            continue;
        setCurrent(e, found, isDirectory);
        if (isDirectory && (e->_options & kCFURLEnumeratorSkipPackageContents) && isPackageName(d->d_name))
            e->_skipCurrent = true;
        if (url != NULL)
            *url = found;
        return kCFURLEnumeratorSuccess;
    }
    return kCFURLEnumeratorEnd;
}

void
CFURLEnumeratorSkipDescendents(CFURLEnumeratorRef enumerator)
{
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)enumerator;
    if (e != NULL)
        e->_skipCurrent = true;
}

/* The level of what was last returned: 1 for the directory's own items. */
CFIndex
CFURLEnumeratorGetDescendentLevel(CFURLEnumeratorRef enumerator)
{
    struct __CFURLEnumerator *e = (struct __CFURLEnumerator *)enumerator;
    if (e == NULL)
        return 0;
    if (e->_volumes != NULL)
        return 1;
    return e->_depth;
}

/* Deprecated: nothing is cached here. */
Boolean
CFURLEnumeratorGetSourceDidChange(CFURLEnumeratorRef enumerator)
{
    return false;
}
