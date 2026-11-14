/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "CTFontManager.h"
#include <fontconfig/fontconfig.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>

const CFStringRef kCTFontManagerErrorDomain = CFSTR("com.apple.CoreText.CTFontManagerErrorDomain");
const CFStringRef kCTFontManagerErrorFontURLsKey = CFSTR("CTFontManagerErrorFontURLs");

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableSetRef registered;	/* the paths registered */

/* CoreText's fontconfig configuration: the one in its resources (which
   any process, sandboxed or not, may read), with its cache in the user's
   cache directory, where sandbox profiles let font caches be read
   (com.apple.FontRegistry). A process that may not write there reads the
   cache another wrote. FONTCONFIG_FILE, when set, is used instead. */
static const char kConfigFile[] = "/System/Library/Frameworks/CoreText.framework/Resources/fonts.conf";
static const char kSystemCache[] = "/System/Library/Caches/com.apple.FontRegistry/fontconfig";

static FcConfig *loadConfig(void) {
    if (getenv("FONTCONFIG_FILE") != NULL)
        return FcInitLoadConfigAndFonts();

    FcConfig *config = FcConfigCreate();
    if (config == NULL)
        return NULL;
    char cache[PATH_MAX] = "";
    size_t n = confstr(_CS_DARWIN_USER_CACHE_DIR, cache, sizeof(cache));
    if (n > 0 && n < sizeof(cache) - 64) {
        strlcat(cache, "com.apple.FontRegistry", sizeof(cache));
        mkdir(cache, 0700);
        strlcat(cache, "/fontconfig", sizeof(cache));
        mkdir(cache, 0700);
    } else
        cache[0] = '\0';

    /* The system's cache first: every process may read it, sandboxed ones
       too (their profiles allow /System), and root (fontcache, at boot)
       writes it; then the user's, which the others write. Fontconfig
       reads all its cache directories and writes the first it can. */
    if (geteuid() == 0) {
        mkdir("/System/Library/Caches", 0755);
        mkdir("/System/Library/Caches/com.apple.FontRegistry", 0755);
        mkdir(kSystemCache, 0755);
        /* Root keeps only the system's: a cache of its own would stand in
           for it, and the system's would never be written. */
        cache[0] = '\0';
    }
    char xml[3 * PATH_MAX + 256];
    snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?><fontconfig><include>%s</include><cachedir>%s</cachedir>%s%s%s</fontconfig>",
        kConfigFile, kSystemCache, cache[0] ? "<cachedir>" : "", cache, cache[0] ? "</cachedir>" : "");
    if (!FcConfigParseAndLoadFromMemory(config, (const FcChar8 *)xml, FcTrue) || !FcConfigBuildFonts(config)) {
        FcConfigDestroy(config);
        return FcInitLoadConfigAndFonts();
    }
    FcConfigSetCurrent(config);
    return config;
}

void *_CTFontManagerSharedFontConfig(void) {
    static FcConfig *config;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        config = loadConfig();
        registered = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    });
    return config;
}

static CFErrorRef createError(CFIndex code, CFURLRef url) {
    const void *keys[] = { kCTFontManagerErrorFontURLsKey };
    CFArrayRef urls = CFArrayCreate(kCFAllocatorDefault, (const void **)&url, 1, &kCFTypeArrayCallBacks);
    const void *values[] = { urls };
    CFErrorRef error = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, kCTFontManagerErrorDomain, code,
        keys, values, 1);
    CFRelease(urls);
    return error;
}

static void postChanged(void) {
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(),
        kCTFontManagerRegisteredFontsChangedNotification, NULL, NULL, true);
}

/* One file, for the process; no error is NULL. */
static CFErrorRef registerOne(CFURLRef url) {
    char path[PATH_MAX];
    if (url == NULL || !CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path)))
        return createError(kCTFontManagerErrorFileNotFound, url);
    if (access(path, F_OK) != 0)
        return createError(kCTFontManagerErrorFileNotFound, url);
    if (access(path, R_OK) != 0)
        return createError(kCTFontManagerErrorInsufficientPermissions, url);
    FcConfig *config = _CTFontManagerSharedFontConfig();
    CFStringRef key = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    CFErrorRef error = NULL;
    pthread_mutex_lock(&lock);
    if (CFSetContainsValue(registered, key)) {
        error = createError(kCTFontManagerErrorAlreadyRegistered, url);
    } else {
        /* A file fontconfig can read gives at least one face. */
        int faces = 0;
        FcBlanks *blanks = NULL;
        FcFontSet *set = FcFontSetCreate();
        if (set != NULL) {
            FcFileScan(set, NULL, NULL, blanks, (const FcChar8 *)path, FcTrue);
            faces = set->nfont;
            FcFontSetDestroy(set);
        }
        if (faces == 0)
            error = createError(kCTFontManagerErrorUnrecognizedFormat, url);
        else if (!FcConfigAppFontAddFile(config, (const FcChar8 *)path))
            error = createError(kCTFontManagerErrorInvalidFontData, url);
        else
            CFSetAddValue(registered, key);
    }
    pthread_mutex_unlock(&lock);
    CFRelease(key);
    return error;
}

bool CTFontManagerRegisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error) {
    CFErrorRef e = registerOne(fontURL);
    if (e == NULL) {
        postChanged();
        if (error != NULL)
            *error = NULL;
        return true;
    }
    if (error != NULL)
        *error = e;
    else
        CFRelease(e);
    return false;
}

/* fontconfig cannot drop one file from a configuration: the registration
   is forgotten, and the fonts stay until the process ends. */
bool CTFontManagerUnregisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error) {
    char path[PATH_MAX];
    bool removed = false;
    _CTFontManagerSharedFontConfig();
    if (fontURL != NULL && CFURLGetFileSystemRepresentation(fontURL, true, (UInt8 *)path, sizeof(path))) {
        CFStringRef key = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
        pthread_mutex_lock(&lock);
        removed = CFSetContainsValue(registered, key);
        CFSetRemoveValue(registered, key);
        pthread_mutex_unlock(&lock);
        CFRelease(key);
    }
    if (!removed && error != NULL)
        *error = createError(kCTFontManagerErrorNotRegistered, fontURL);
    return removed;
}

void CTFontManagerRegisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool enabled,
                                   bool (^handler)(CFArrayRef errors, bool done)) {
    CFMutableArrayRef errors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFIndex count = fontURLs != NULL ? CFArrayGetCount(fontURLs) : 0;
    bool any = false;
    for (CFIndex i = 0; i < count; i++) {
        CFErrorRef e = registerOne(CFArrayGetValueAtIndex(fontURLs, i));
        if (e != NULL) {
            CFArrayAppendValue(errors, e);
            CFRelease(e);
        } else {
            any = true;
        }
    }
    if (any)
        postChanged();
    if (handler != NULL)
        handler(errors, true);
    CFRelease(errors);
}

void CTFontManagerUnregisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope,
                                     bool (^handler)(CFArrayRef errors, bool done)) {
    CFMutableArrayRef errors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0, n = fontURLs != NULL ? CFArrayGetCount(fontURLs) : 0; i < n; i++) {
        CFErrorRef e = NULL;
        if (!CTFontManagerUnregisterFontsForURL(CFArrayGetValueAtIndex(fontURLs, i), scope, &e) && e != NULL) {
            CFArrayAppendValue(errors, e);
            CFRelease(e);
        }
    }
    if (handler != NULL)
        handler(errors, true);
    CFRelease(errors);
}

static CFArrayRef copyNames(const char *object) {
    FcConfig *config = _CTFontManagerSharedFontConfig();
    FcPattern *pattern = FcPatternCreate();
    FcObjectSet *objects = FcObjectSetBuild(object, (char *)0);
    pthread_mutex_lock(&lock);
    FcFontSet *set = FcFontList(config, pattern, objects);
    pthread_mutex_unlock(&lock);
    CFMutableSetRef seen = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    for (int i = 0; set != NULL && i < set->nfont; i++) {
        FcChar8 *name;
        if (FcPatternGetString(set->fonts[i], object, 0, &name) == FcResultMatch) {
            CFStringRef s = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)name, kCFStringEncodingUTF8);
            if (s != NULL) {
                CFSetAddValue(seen, s);
                CFRelease(s);
            }
        }
    }
    if (set != NULL)
        FcFontSetDestroy(set);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(pattern);
    CFIndex n = CFSetGetCount(seen);
    const void **values = malloc((n > 0 ? n : 1) * sizeof(void *));
    CFSetGetValues(seen, values);
    CFMutableArrayRef names = CFArrayCreateMutable(kCFAllocatorDefault, n, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < n; i++)
        CFArrayAppendValue(names, values[i]);
    free(values);
    CFRelease(seen);
    CFArraySortValues(names, CFRangeMake(0, n), (CFComparatorFunction)CFStringCompare, NULL);
    return names;
}

CFArrayRef CTFontManagerCopyAvailableFontFamilyNames(void) {
    return copyNames(FC_FAMILY);
}

CFArrayRef CTFontManagerCopyAvailablePostScriptNames(void) {
    return copyNames(FC_POSTSCRIPT_NAME);
}
