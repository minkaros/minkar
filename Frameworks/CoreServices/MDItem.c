/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Metadata items: a path, and its attributes read when they are asked
 * for (from the file system, its content type from LaunchServices, and an
 * application's bundle).
 */

#include "CoreServices.h"
#include <CoreFoundation/CFRuntime.h>
#include <limits.h>
#include <dispatch/dispatch.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

struct __MDItem {
    CFRuntimeBase base;
    CFStringRef path;
};

static CFTypeID sItemTypeID = 0;

static void itemFinalize(CFTypeRef cf) {
    MDItemRef item = (MDItemRef)cf;
    if (item->path != NULL)
        CFRelease(item->path);
}

static CFStringRef itemDescription(CFTypeRef cf) {
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<MDItem %p [%@]>"), cf, ((MDItemRef)cf)->path);
}

static const CFRuntimeClass sItemClass = {
    0, "MDItem", NULL, NULL, itemFinalize, NULL, NULL, NULL, itemDescription
};

CFTypeID MDItemGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ sItemTypeID = _CFRuntimeRegisterClass(&sItemClass); });
    return sItemTypeID;
}

MDItemRef MDItemCreate(CFAllocatorRef allocator, CFStringRef path) {
    char buffer[PATH_MAX];
    struct stat st;
    MDItemRef item;

    if (path == NULL || !CFStringGetFileSystemRepresentation(path, buffer, sizeof(buffer)) || lstat(buffer, &st) != 0)
        return NULL;
    item = (MDItemRef)_CFRuntimeCreateInstance(allocator, MDItemGetTypeID(),
                                               sizeof(struct __MDItem) - sizeof(CFRuntimeBase), NULL);
    if (item != NULL)
        item->path = CFStringCreateCopy(NULL, path);
    return item;
}

MDItemRef MDItemCreateWithURL(CFAllocatorRef allocator, CFURLRef url) {
    CFStringRef path;
    MDItemRef item;

    if (url == NULL)
        return NULL;
    path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (path == NULL)
        return NULL;
    item = MDItemCreate(allocator, path);
    CFRelease(path);
    return item;
}

static CFDateRef dateOf(struct timespec t) {
    return CFDateCreate(NULL, (CFAbsoluteTime)t.tv_sec - kCFAbsoluteTimeIntervalSince1970 + t.tv_nsec / 1e9);
}

static CFStringRef contentTypeOf(CFStringRef path, Boolean directory) {
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, directory);
    CFStringRef type = NULL;
    if (url != NULL) {
        CFStringRef extension = CFURLCopyPathExtension(url);
        if (directory && (extension == NULL || CFStringGetLength(extension) == 0))
            type = CFRetain(CFSTR("public.folder"));
        else if (extension != NULL)
            type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension,
                                                         directory ? CFSTR("public.directory") : CFSTR("public.data"));
        else
            type = CFRetain(CFSTR("public.data"));
        if (extension != NULL)
            CFRelease(extension);
        CFRelease(url);
    }
    return type;
}

/* The type and the types it conforms to, nearest first. */
static CFArrayRef typeTreeOf(CFStringRef type) {
    CFMutableArrayRef tree = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableArrayRef queue = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(queue, type);
    while (CFArrayGetCount(queue) > 0) {
        CFStringRef t = CFRetain(CFArrayGetValueAtIndex(queue, 0));
        CFArrayRemoveValueAtIndex(queue, 0);
        if (!CFArrayContainsValue(tree, CFRangeMake(0, CFArrayGetCount(tree)), t)) {
            CFArrayAppendValue(tree, t);
            CFDictionaryRef declaration = UTTypeCopyDeclaration(t);
            if (declaration != NULL) {
                CFTypeRef parents = CFDictionaryGetValue(declaration, kUTTypeConformsToKey);
                if (parents != NULL && CFGetTypeID(parents) == CFArrayGetTypeID())
                    CFArrayAppendArray(queue, parents, CFRangeMake(0, CFArrayGetCount(parents)));
                else if (parents != NULL && CFGetTypeID(parents) == CFStringGetTypeID())
                    CFArrayAppendValue(queue, parents);
                CFRelease(declaration);
            }
        }
        CFRelease(t);
    }
    CFRelease(queue);
    return tree;
}

/* A package (an app, a bundle the type system calls one): a file to the
   user, so its size is its files'. */
static Boolean isPackage(CFStringRef path) {
    CFStringRef type = contentTypeOf(path, true);
    Boolean package = type != NULL && (UTTypeConformsTo(type, CFSTR("com.apple.package")) ||
                                       UTTypeConformsTo(type, CFSTR("com.apple.bundle")));
    if (type != NULL)
        CFRelease(type);
    return package;
}

/* The sizes of a directory's files, all the way down (logical, and the
   blocks they take). */
static void addSizes(const char *path, long long *logical, long long *physical, int depth) {
    DIR *dir = depth < 64 ? opendir(path) : NULL;
    struct dirent *entry;
    if (dir == NULL)
        return;
    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) || lstat(child, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            addSizes(child, logical, physical, depth + 1);
        else {
            *logical += st.st_size;
            *physical += (long long)st.st_blocks * 512;
        }
    }
    closedir(dir);
}

static CFNumberRef numberOf(long long value) {
    return CFNumberCreate(NULL, kCFNumberLongLongType, &value);
}

static CFTypeRef bundleValue(CFStringRef path, CFStringRef key) {
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, true);
    CFBundleRef bundle = url != NULL ? CFBundleCreate(NULL, url) : NULL;
    CFTypeRef value = NULL;
    if (bundle != NULL) {
        value = CFBundleGetValueForInfoDictionaryKey(bundle, key);
        if (value != NULL)
            CFRetain(value);
        CFRelease(bundle);
    }
    if (url != NULL)
        CFRelease(url);
    return value;
}

CFTypeRef MDItemCopyAttribute(MDItemRef item, CFStringRef name) {
    char buffer[PATH_MAX];
    struct stat st;

    if (item == NULL || name == NULL)
        return NULL;
    if (!CFStringGetFileSystemRepresentation(item->path, buffer, sizeof(buffer)) || lstat(buffer, &st) != 0)
        return NULL;
    Boolean directory = S_ISDIR(st.st_mode);
    if (CFEqual(name, kMDItemPath))
        return CFRetain(item->path);
    if (CFEqual(name, kMDItemFSName) || CFEqual(name, kMDItemDisplayName)) {
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, item->path, kCFURLPOSIXPathStyle, directory);
        CFStringRef last = url != NULL ? CFURLCopyLastPathComponent(url) : NULL;
        if (url != NULL)
            CFRelease(url);
        if (last != NULL && CFEqual(name, kMDItemDisplayName) && directory && CFStringHasSuffix(last, CFSTR(".app"))) {
            /* An app by the name it shows (its bundle's), else its file's
               without the extension. */
            CFTypeRef bundleName = bundleValue(item->path, CFSTR("CFBundleDisplayName"));
            if (bundleName == NULL || CFGetTypeID(bundleName) != CFStringGetTypeID()) {
                if (bundleName != NULL)
                    CFRelease(bundleName);
                bundleName = bundleValue(item->path, kCFBundleNameKey);
            }
            if (bundleName != NULL && CFGetTypeID(bundleName) == CFStringGetTypeID() && CFStringGetLength(bundleName) > 0) {
                CFRelease(last);
                return bundleName;
            }
            if (bundleName != NULL)
                CFRelease(bundleName);
            CFStringRef shown = CFStringCreateWithSubstring(NULL, last, CFRangeMake(0, CFStringGetLength(last) - 4));
            CFRelease(last);
            return shown;
        }
        return last;
    }
    if (CFEqual(name, kMDItemFSSize) || CFEqual(name, kMDItemLogicalSize) || CFEqual(name, kMDItemPhysicalSize)) {
        long long logical = (long long)st.st_size, physical = (long long)st.st_blocks * 512;
        /* A folder has no size; a package, its files'. */
        if (directory && isPackage(item->path)) {
            logical = physical = 0;
            addSizes(buffer, &logical, &physical, 0);
        } else if (directory)
            return NULL;
        return numberOf(CFEqual(name, kMDItemPhysicalSize) ? physical : logical);
    }
    /* When it was put where it is: its inode's last change, the nearest
       the file system keeps. */
    if (CFEqual(name, kMDItemDateAdded))
        return dateOf(st.st_ctimespec);
    if (CFEqual(name, kMDItemFSNodeCount)) {
        DIR *dir = directory ? opendir(buffer) : NULL;
        long long count = 0;
        struct dirent *entry;
        if (dir == NULL)
            return NULL;
        while ((entry = readdir(dir)) != NULL)
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                count++;
        closedir(dir);
        return numberOf(count);
    }
    /* The Finder's own fields: none kept here. */
    if (CFEqual(name, kMDItemFSLabel) || CFEqual(name, kMDItemFSFinderFlags) || CFEqual(name, kMDItemDocumentIdentifier))
        return numberOf(0);
    if (CFEqual(name, kMDItemFSCreatorCode) || CFEqual(name, kMDItemFSTypeCode))
        return CFRetain(CFSTR(""));
    if (directory && CFEqual(name, kMDItemCopyright))
        return bundleValue(item->path, CFSTR("NSHumanReadableCopyright"));
    if (directory && CFStringHasSuffix(item->path, CFSTR(".app")) &&
        (CFEqual(name, kMDItemExecutableArchitectures) || CFEqual(name, kMDItemAlternateNames))) {
        CFTypeRef value;
        if (CFEqual(name, kMDItemExecutableArchitectures))
            value = CFSTR("x86_64");
        else {
            const char *slash = strrchr(buffer, '/');
            value = CFStringCreateWithFileSystemRepresentation(NULL, slash != NULL ? slash + 1 : buffer);
        }
        CFArrayRef result = CFArrayCreate(NULL, &value, 1, &kCFTypeArrayCallBacks);
        if (CFEqual(name, kMDItemAlternateNames))
            CFRelease(value);
        return result;
    }
    if (CFEqual(name, kMDItemFSCreationDate) || CFEqual(name, kMDItemContentCreationDate))
        return dateOf(st.st_birthtimespec);
    if (CFEqual(name, kMDItemFSContentChangeDate) || CFEqual(name, kMDItemContentModificationDate))
        return dateOf(st.st_mtimespec);
    if (CFEqual(name, kMDItemLastUsedDate))
        return dateOf(st.st_atimespec);
    if (CFEqual(name, kMDItemFSOwnerUserID)) {
        int value = (int)st.st_uid;
        return CFNumberCreate(NULL, kCFNumberIntType, &value);
    }
    if (CFEqual(name, kMDItemFSOwnerGroupID)) {
        int value = (int)st.st_gid;
        return CFNumberCreate(NULL, kCFNumberIntType, &value);
    }
    if (CFEqual(name, kMDItemFSInvisible)) {
        const char *slash = strrchr(buffer, '/');
        Boolean hidden = (slash != NULL && slash[1] == '.') || (st.st_flags & UF_HIDDEN);
        return CFRetain(hidden ? kCFBooleanTrue : kCFBooleanFalse);
    }
    /* An app's name is shown without its extension. */
    if (CFEqual(name, kMDItemFSIsExtensionHidden))
        return CFRetain(directory && CFStringHasSuffix(item->path, CFSTR(".app")) ? kCFBooleanTrue : kCFBooleanFalse);
    if (CFEqual(name, kMDItemContentType))
        return contentTypeOf(item->path, directory);
    if (CFEqual(name, kMDItemContentTypeTree) || CFEqual(name, kMDItemKind)) {
        CFStringRef type = contentTypeOf(item->path, directory);
        CFTypeRef result = NULL;
        if (type != NULL) {
            if (CFEqual(name, kMDItemKind)) {
                /* The kind as shown: "Plain Text Document". */
                CFStringRef description = UTTypeCopyDescription(type);
                if (description != NULL) {
                    CFMutableStringRef kind = CFStringCreateMutableCopy(NULL, 0, description);
                    CFStringCapitalize(kind, NULL);
                    CFRelease(description);
                    result = kind;
                }
            } else
                result = typeTreeOf(type);
            CFRelease(type);
        }
        return result;
    }
    if (directory && CFEqual(name, kMDItemCFBundleIdentifier))
        return bundleValue(item->path, kCFBundleIdentifierKey);
    if (directory && CFEqual(name, kMDItemVersion))
        return bundleValue(item->path, CFSTR("CFBundleShortVersionString"));
    return NULL;
}

CFDictionaryRef MDItemCopyAttributes(MDItemRef item, CFArrayRef names) {
    CFMutableDictionaryRef result;

    if (item == NULL || names == NULL)
        return NULL;
    result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (CFIndex i = 0, n = CFArrayGetCount(names); i < n; i++) {
        CFStringRef name = CFArrayGetValueAtIndex(names, i);
        CFTypeRef value = MDItemCopyAttribute(item, name);
        if (value != NULL) {
            CFDictionarySetValue(result, name, value);
            CFRelease(value);
        }
    }
    return result;
}

CFArrayRef MDItemCopyAttributeNames(MDItemRef item) {
    const void *names[] = { kMDItemPath, kMDItemFSName, kMDItemDisplayName, kMDItemFSSize, kMDItemFSCreationDate,
                            kMDItemFSContentChangeDate, kMDItemContentCreationDate, kMDItemContentModificationDate,
                            kMDItemLastUsedDate, kMDItemFSOwnerUserID, kMDItemFSOwnerGroupID, kMDItemFSInvisible,
                            kMDItemFSIsExtensionHidden, kMDItemContentType, kMDItemContentTypeTree, kMDItemKind,
                            kMDItemLogicalSize, kMDItemPhysicalSize, kMDItemDateAdded, kMDItemFSNodeCount,
                            kMDItemFSLabel, kMDItemFSFinderFlags, kMDItemFSCreatorCode, kMDItemFSTypeCode,
                            kMDItemDocumentIdentifier, kMDItemCFBundleIdentifier, kMDItemVersion, kMDItemCopyright,
                            kMDItemExecutableArchitectures, kMDItemAlternateNames };
    if (item == NULL)
        return NULL;
    /* Those the item has. */
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        CFTypeRef value = MDItemCopyAttribute(item, names[i]);
        if (value != NULL) {
            CFArrayAppendValue(result, names[i]);
            CFRelease(value);
        }
    }
    return result;
}

/* The attributes named, up to a NULL. */
CFDictionaryRef MDItemCopyAttributeList(MDItemRef item, ...) {
    CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    va_list ap;
    CFStringRef name;
    va_start(ap, item);
    while ((name = va_arg(ap, CFStringRef)) != NULL)
        CFArrayAppendValue(names, name);
    va_end(ap);
    CFDictionaryRef result = MDItemCopyAttributes(item, names);
    CFRelease(names);
    return result;
}
