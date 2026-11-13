/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A constant's value is its name without "k".
 */

#include "TextInputSources.h"
#include <stdio.h>
#include <stdlib.h>

#define KEY(name) const CFStringRef k##name = CFSTR(#name)

KEY(TISPropertyInputSourceCategory);
KEY(TISPropertyInputSourceType);
KEY(TISPropertyInputSourceIsASCIICapable);
KEY(TISPropertyInputSourceIsEnableCapable);
KEY(TISPropertyInputSourceIsSelectCapable);
KEY(TISPropertyInputSourceIsEnabled);
KEY(TISPropertyInputSourceIsSelected);
KEY(TISPropertyInputSourceID);
KEY(TISPropertyBundleID);
KEY(TISPropertyInputModeID);
KEY(TISPropertyLocalizedName);
KEY(TISPropertyInputSourceLanguages);
KEY(TISPropertyUnicodeKeyLayoutData);
KEY(TISPropertyIconImageURL);
KEY(TISCategoryKeyboardInputSource);
KEY(TISCategoryPaletteInputSource);
KEY(TISCategoryInkInputSource);
KEY(TISTypeKeyboardLayout);
KEY(TISTypeKeyboardInputMethodWithoutModes);
KEY(TISTypeKeyboardInputMethodModeEnabled);
KEY(TISTypeKeyboardInputMode);
KEY(TISTypeCharacterPalette);
KEY(TISTypeKeyboardViewer);
KEY(TISTypeInk);
KEY(TISNotifySelectedKeyboardInputSourceChanged);
KEY(TISNotifyEnabledKeyboardInputSourcesChanged);

/*
 * The input sources: Minkar's keyboard layout in use (the window server's,
 * SLSGetKeyboardSettings), as one keyboard layout source. Its layout data
 * is CoreServices' MinkarKeyboardLayoutData, which UCKeyTranslate takes.
 */

#include <CoreFoundation/CFRuntime.h>
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <string.h>

typedef struct {
    char layout[32];
    char variant[32];
    uint32_t initialRepeat;
    uint32_t repeatInterval;
} KeyboardSettings;

struct __TISInputSource {
    CFRuntimeBase base;
    CFDictionaryRef properties;
};

static CFTypeID inputSourceTypeID;

static void inputSourceFinalize(CFTypeRef cf) {
    struct __TISInputSource *s = (struct __TISInputSource *)cf;
    if (s->properties != NULL)
        CFRelease(s->properties);
}

static CFStringRef inputSourceDescription(CFTypeRef cf) {
    struct __TISInputSource *s = (struct __TISInputSource *)cf;
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<TISInputSource %p>{%@}"), cf,
        CFDictionaryGetValue(s->properties, kTISPropertyInputSourceID));
}

static const CFRuntimeClass inputSourceClass = {
    0, "TISInputSource", NULL, NULL, inputSourceFinalize, NULL, NULL, NULL, inputSourceDescription
};

CFTypeID TISInputSourceGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ inputSourceTypeID = _CFRuntimeRegisterClass(&inputSourceClass); });
    return inputSourceTypeID;
}

static void currentSettings(KeyboardSettings *settings) {
    typedef int (*GetFn)(KeyboardSettings *);
    static GetFn get;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        get = (GetFn)dlsym(RTLD_DEFAULT, "SLSGetKeyboardSettings");
        if (get == NULL) {
            void *h = dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight", RTLD_LAZY);
            if (h != NULL)
                get = (GetFn)dlsym(h, "SLSGetKeyboardSettings");
        }
    });
    memset(settings, 0, sizeof(*settings));
    if (get == NULL || get(settings) != 0 || settings->layout[0] == '\0')
        strlcpy(settings->layout, "us", sizeof(settings->layout));
}

/* A layout's language (the languages it types): its code when it is one. */
static CFStringRef languageOfLayout(const char *layout) {
    static const struct { const char *layout, *language; } table[] = {
        { "us", "en" }, { "gb", "en" }, { "uk", "en" }, { "ca", "fr" }, { "latam", "es" }, { "br", "pt" },
        { "se", "sv" }, { "dk", "da" }, { "cz", "cs" }, { "ua", "uk" }, { "gr", "el" }, { "jp", "ja" },
        { "kr", "ko" }, { "cn", "zh" }, { "il", "he" }, { "ir", "fa" }, { "ch", "de" }, { "at", "de" },
        { "be", "fr" }, { "rs", "sr" }, { "si", "sl" }, { "ee", "et" }, { "by", "be" }, { "kz", "kk" },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(layout, table[i].layout) == 0)
            return CFStringCreateWithCString(NULL, table[i].language, kCFStringEncodingUTF8);
    return CFStringCreateWithCString(NULL, layout, kCFStringEncodingUTF8);
}

static Boolean asciiCapable(const char *layout) {
    static const char *const nonLatin[] = { "ru", "ua", "gr", "il", "ir", "ara", "th", "by", "kz", "bg", "mk",
        "rs", "jp", "kr", "cn", "in", "am", "ge", NULL };
    for (int i = 0; nonLatin[i] != NULL; i++)
        if (strcmp(layout, nonLatin[i]) == 0)
            return false;
    return true;
}

static TISInputSourceRef createSource(const KeyboardSettings *settings) {
    struct __TISInputSource *s = (struct __TISInputSource *)_CFRuntimeCreateInstance(NULL,
        TISInputSourceGetTypeID(), sizeof(struct __TISInputSource) - sizeof(CFRuntimeBase), NULL);
    if (s == NULL)
        return NULL;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    char name[80];
    if (settings->variant[0] != '\0')
        snprintf(name, sizeof(name), "%s-%s", settings->layout, settings->variant);
    else
        snprintf(name, sizeof(name), "%s", settings->layout);
    CFStringRef layoutName = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    CFStringRef sourceID = CFStringCreateWithFormat(NULL, NULL, CFSTR("org.minkar.keylayout.%@"), layoutName);
    CFDictionarySetValue(d, kTISPropertyInputSourceID, sourceID);
    CFDictionarySetValue(d, kTISPropertyLocalizedName, layoutName);
    CFDictionarySetValue(d, kTISPropertyInputSourceCategory, kTISCategoryKeyboardInputSource);
    CFDictionarySetValue(d, kTISPropertyInputSourceType, kTISTypeKeyboardLayout);
    CFDictionarySetValue(d, kTISPropertyBundleID, CFSTR("org.minkar.keylayout"));
    Boolean ascii = asciiCapable(settings->layout);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsASCIICapable, ascii ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsEnabled, kCFBooleanTrue);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsSelected, kCFBooleanTrue);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsEnableCapable, kCFBooleanTrue);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsSelectCapable, kCFBooleanTrue);
    CFStringRef language = languageOfLayout(settings->layout);
    CFArrayRef languages = CFArrayCreate(NULL, (const void **)&language, 1, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(d, kTISPropertyInputSourceLanguages, languages);
    MinkarKeyboardLayoutData data;
    memset(&data, 0, sizeof(data));
    data.magic = kMinkarKeyboardLayoutMagic;
    data.version = 1;
    strlcpy(data.layout, settings->layout, sizeof(data.layout));
    strlcpy(data.variant, settings->variant, sizeof(data.variant));
    CFDataRef layoutData = CFDataCreate(NULL, (const UInt8 *)&data, sizeof(data));
    CFDictionarySetValue(d, kTISPropertyUnicodeKeyLayoutData, layoutData);
    CFRelease(layoutData);
    CFRelease(languages);
    CFRelease(language);
    CFRelease(sourceID);
    CFRelease(layoutName);
    s->properties = d;
    return (TISInputSourceRef)s;
}

void *TISGetInputSourceProperty(TISInputSourceRef inputSource, CFStringRef propertyKey) {
    if (inputSource == NULL || propertyKey == NULL)
        return NULL;
    return (void *)CFDictionaryGetValue(((struct __TISInputSource *)inputSource)->properties, propertyKey);
}

TISInputSourceRef TISCopyCurrentKeyboardInputSource(void) {
    KeyboardSettings settings;
    currentSettings(&settings);
    return createSource(&settings);
}

TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void) {
    return TISCopyCurrentKeyboardInputSource();
}

/* With a non-Latin layout in use, the US layout is the ASCII-capable one. */
TISInputSourceRef TISCopyCurrentASCIICapableKeyboardInputSource(void) {
    KeyboardSettings settings;
    currentSettings(&settings);
    if (!asciiCapable(settings.layout)) {
        strlcpy(settings.layout, "us", sizeof(settings.layout));
        settings.variant[0] = '\0';
    }
    return createSource(&settings);
}

TISInputSourceRef TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void) {
    return TISCopyCurrentASCIICapableKeyboardInputSource();
}

TISInputSourceRef TISCopyInputMethodKeyboardLayoutOverride(void) {
    return NULL;
}

static Boolean matches(TISInputSourceRef source, CFDictionaryRef properties) {
    Boolean ok = true;
    if (properties == NULL)
        return true;
    CFIndex n = CFDictionaryGetCount(properties);
    const void **keys = malloc((n > 0 ? n : 1) * sizeof(void *) * 2);
    const void **values = keys + (n > 0 ? n : 1);
    CFDictionaryGetKeysAndValues(properties, keys, values);
    for (CFIndex i = 0; i < n && ok; i++) {
        CFTypeRef mine = TISGetInputSourceProperty(source, keys[i]);
        ok = mine != NULL && CFEqual(mine, values[i]);
    }
    free(keys);
    return ok;
}

CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    if (current != NULL) {
        if (matches(current, properties))
            CFArrayAppendValue(list, current);
        CFRelease(current);
    }
    return list;
}

CFArrayRef TISCreateASCIICapableInputSourceList(void) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    TISInputSourceRef source = TISCopyCurrentASCIICapableKeyboardInputSource();
    if (source != NULL) {
        CFArrayAppendValue(list, source);
        CFRelease(source);
    }
    return list;
}

TISInputSourceRef TISCopyInputSourceForLanguage(CFStringRef language) {
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    CFArrayRef languages = current != NULL ? TISGetInputSourceProperty(current, kTISPropertyInputSourceLanguages) : NULL;
    if (language != NULL && languages != NULL &&
        CFArrayContainsValue(languages, CFRangeMake(0, CFArrayGetCount(languages)), language))
        return current;
    if (current != NULL)
        CFRelease(current);
    return NULL;
}

/* There is one source, already selected and enabled; another layout is
   chosen in System Preferences. */
OSStatus TISSelectInputSource(TISInputSourceRef inputSource) {
    return inputSource != NULL ? noErr : paramErr;
}

OSStatus TISDeselectInputSource(TISInputSourceRef inputSource) {
    return paramErr;
}

OSStatus TISEnableInputSource(TISInputSourceRef inputSource) {
    return inputSource != NULL ? noErr : paramErr;
}

OSStatus TISDisableInputSource(TISInputSourceRef inputSource) {
    return paramErr;
}
