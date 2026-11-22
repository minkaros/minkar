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
 * The input sources: Minkar's keyboard layouts (the ones the Keyboard pane
 * offers), each a keyboard layout source named as on the reference
 * platform. The enabled ones are kept as the reference keeps them, in
 * com.apple.HIToolbox's AppleEnabledInputSources (with the selected one in
 * AppleSelectedInputSources and AppleCurrentKeyboardLayoutInputSourceID);
 * the selected one is the window server's layout (SLSGetKeyboardSettings,
 * SLSSetKeyboardSettings). Changes are posted as the reference's
 * distributed notifications. A source's layout data is CoreServices'
 * MinkarKeyboardLayoutData, which UCKeyTranslate takes.
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

/* The layouts installed: XKB layout and variant, the reference's name. */
static const struct { const char *layout, *variant, *name; } kLayouts[] = {
    { "us", "", "U.S." },
    { "gb", "", "British" },
    { "es", "", "Spanish - ISO" },
    { "latam", "", "Latin American" },
    { "br", "", "Brazilian" },
    { "fr", "", "French" },
    { "de", "", "German" },
    { "it", "", "Italian" },
};
#define kLayoutCount (sizeof(kLayouts) / sizeof(kLayouts[0]))
#define kHIToolbox CFSTR("com.apple.HIToolbox")

static TISInputSourceRef createLayoutSource(size_t i, Boolean enabled, Boolean selected);
static long currentLayoutIndex(void);

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

/* SkyLight's function by name (it is not linked: Carbon is below it). */
static void *skyLight(const char *name) {
    void *f = dlsym(RTLD_DEFAULT, name);
    if (f == NULL) {
        void *h = dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight", RTLD_LAZY);
        if (h != NULL)
            f = dlsym(h, name);
    }
    return f;
}

static void currentSettings(KeyboardSettings *settings) {
    typedef int (*GetFn)(KeyboardSettings *);
    static GetFn get;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ get = (GetFn)skyLight("SLSGetKeyboardSettings"); });
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
    char name[80], ident[80];
    if (settings->variant[0] != '\0')
        snprintf(ident, sizeof(ident), "%s-%s", settings->layout, settings->variant);
    else
        snprintf(ident, sizeof(ident), "%s", settings->layout);
    snprintf(name, sizeof(name), "%s", ident);
    for (size_t i = 0; i < kLayoutCount; i++)
        if (strcmp(kLayouts[i].layout, settings->layout) == 0 && strcmp(kLayouts[i].variant, settings->variant) == 0)
            snprintf(name, sizeof(name), "%s", kLayouts[i].name);
    CFStringRef layoutName = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    CFStringRef identString = CFStringCreateWithCString(NULL, ident, kCFStringEncodingUTF8);
    CFStringRef sourceID = CFStringCreateWithFormat(NULL, NULL, CFSTR("org.minkar.keylayout.%@"), identString);
    CFRelease(identString);
    CFDictionarySetValue(d, kTISPropertyInputSourceID, sourceID);
    CFDictionarySetValue(d, kTISPropertyLocalizedName, layoutName);
    CFDictionarySetValue(d, kTISPropertyInputSourceCategory, kTISCategoryKeyboardInputSource);
    CFDictionarySetValue(d, kTISPropertyInputSourceType, kTISTypeKeyboardLayout);
    CFDictionarySetValue(d, kTISPropertyBundleID, CFSTR("org.minkar.keylayout"));
    Boolean ascii = asciiCapable(settings->layout);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsASCIICapable, ascii ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsEnabled, kCFBooleanTrue);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsSelected, kCFBooleanFalse);
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
    long i = currentLayoutIndex();
    if (i >= 0)
        return createLayoutSource((size_t)i, true, true);
    struct __TISInputSource *s = (struct __TISInputSource *)createSource(&settings);
    if (s != NULL) {
        CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, s->properties);
        CFDictionarySetValue(d, kTISPropertyInputSourceIsSelected, kCFBooleanTrue);
        CFRelease(s->properties);
        s->properties = d;
    }
    return (TISInputSourceRef)s;
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

/* A source of an installed layout, its enabled and selected state set. */
static TISInputSourceRef createLayoutSource(size_t i, Boolean enabled, Boolean selected) {
    KeyboardSettings settings;
    memset(&settings, 0, sizeof(settings));
    strlcpy(settings.layout, kLayouts[i].layout, sizeof(settings.layout));
    strlcpy(settings.variant, kLayouts[i].variant, sizeof(settings.variant));
    struct __TISInputSource *s = (struct __TISInputSource *)createSource(&settings);
    if (s == NULL)
        return NULL;
    CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, s->properties);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsEnabled, enabled ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kTISPropertyInputSourceIsSelected, selected ? kCFBooleanTrue : kCFBooleanFalse);
    CFRelease(s->properties);
    s->properties = d;
    return (TISInputSourceRef)s;
}

/* An installed layout by the source's layout data, or -1. */
static long layoutIndexOf(TISInputSourceRef source) {
    CFDataRef data = source != NULL ? TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) : NULL;
    if (data == NULL || CFDataGetLength(data) < (CFIndex)sizeof(MinkarKeyboardLayoutData))
        return -1;
    const MinkarKeyboardLayoutData *k = (const MinkarKeyboardLayoutData *)CFDataGetBytePtr(data);
    for (size_t i = 0; i < kLayoutCount; i++)
        if (strcmp(kLayouts[i].layout, k->layout) == 0 && strcmp(kLayouts[i].variant, k->variant) == 0)
            return (long)i;
    return -1;
}

static long currentLayoutIndex(void) {
    KeyboardSettings settings;
    currentSettings(&settings);
    for (size_t i = 0; i < kLayoutCount; i++)
        if (strcmp(kLayouts[i].layout, settings.layout) == 0 && strcmp(kLayouts[i].variant, settings.variant) == 0)
            return (long)i;
    return -1;
}

/* An AppleEnabledInputSources entry, as the reference writes them, with
   Minkar's own key for the layout. */
static CFDictionaryRef entryFor(size_t i) {
    CFStringRef keys[] = { CFSTR("InputSourceKind"), CFSTR("KeyboardLayout Name"), CFSTR("MinkarKeyboardLayout") };
    char ident[80];
    if (kLayouts[i].variant[0] != '\0')
        snprintf(ident, sizeof(ident), "%s-%s", kLayouts[i].layout, kLayouts[i].variant);
    else
        snprintf(ident, sizeof(ident), "%s", kLayouts[i].layout);
    CFStringRef values[] = { CFSTR("Keyboard Layout"),
        CFStringCreateWithCString(NULL, kLayouts[i].name, kCFStringEncodingUTF8),
        CFStringCreateWithCString(NULL, ident, kCFStringEncodingUTF8) };
    CFDictionaryRef d = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 3,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(values[1]);
    CFRelease(values[2]);
    return d;
}

/* The installed layout of an entry (its Minkar key, else its name), or -1. */
static long layoutIndexOfEntry(CFDictionaryRef e) {
    if (e == NULL || CFGetTypeID(e) != CFDictionaryGetTypeID())
        return -1;
    CFStringRef minkar = CFDictionaryGetValue(e, CFSTR("MinkarKeyboardLayout"));
    CFStringRef name = CFDictionaryGetValue(e, CFSTR("KeyboardLayout Name"));
    char buf[80];
    for (size_t i = 0; i < kLayoutCount; i++) {
        char ident[80];
        if (kLayouts[i].variant[0] != '\0')
            snprintf(ident, sizeof(ident), "%s-%s", kLayouts[i].layout, kLayouts[i].variant);
        else
            snprintf(ident, sizeof(ident), "%s", kLayouts[i].layout);
        if (minkar != NULL && CFGetTypeID(minkar) == CFStringGetTypeID() &&
            CFStringGetCString(minkar, buf, sizeof(buf), kCFStringEncodingUTF8) && strcmp(buf, ident) == 0)
            return (long)i;
        if (minkar == NULL && name != NULL && CFGetTypeID(name) == CFStringGetTypeID() &&
            CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) && strcmp(buf, kLayouts[i].name) == 0)
            return (long)i;
    }
    return -1;
}

/* Which installed layouts are enabled: the user's list, else the one in use. */
static void enabledLayouts(Boolean enabled[kLayoutCount]) {
    memset(enabled, 0, kLayoutCount * sizeof(Boolean));
    CFPreferencesAppSynchronize(kHIToolbox);
    CFArrayRef list = CFPreferencesCopyAppValue(CFSTR("AppleEnabledInputSources"), kHIToolbox);
    Boolean any = false;
    if (list != NULL && CFGetTypeID(list) == CFArrayGetTypeID())
        for (CFIndex k = 0; k < CFArrayGetCount(list); k++) {
            long i = layoutIndexOfEntry(CFArrayGetValueAtIndex(list, k));
            if (i >= 0)
                enabled[i] = any = true;
        }
    if (list != NULL)
        CFRelease(list);
    long current = currentLayoutIndex();
    if (current >= 0)
        enabled[current] = true;
    else if (!any)
        enabled[0] = true;
}

static void saveEnabled(const Boolean enabled[kLayoutCount]) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < kLayoutCount; i++)
        if (enabled[i]) {
            CFDictionaryRef e = entryFor(i);
            CFArrayAppendValue(list, e);
            CFRelease(e);
        }
    CFPreferencesSetAppValue(CFSTR("AppleEnabledInputSources"), list, kHIToolbox);
    CFPreferencesAppSynchronize(kHIToolbox);
    CFRelease(list);
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
        kTISNotifyEnabledKeyboardInputSourcesChanged, NULL, NULL, true);
}

CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    Boolean enabled[kLayoutCount];
    long current = currentLayoutIndex();

    enabledLayouts(enabled);
    for (size_t i = 0; i < kLayoutCount; i++) {
        if (!enabled[i] && !includeAllInstalled)
            continue;
        TISInputSourceRef source = createLayoutSource(i, enabled[i], (long)i == current);
        if (source != NULL && matches(source, properties))
            CFArrayAppendValue(list, source);
        if (source != NULL)
            CFRelease(source);
    }
    /* A layout in use that is not one of the installed ones. */
    if (current < 0) {
        TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
        if (source != NULL && matches(source, properties))
            CFArrayAppendValue(list, source);
        if (source != NULL)
            CFRelease(source);
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

/* The source's layout made the window server's (the console user or root
   may), enabled, and remembered as the reference does. */
OSStatus TISSelectInputSource(TISInputSourceRef inputSource) {
    typedef int (*SetFn)(const KeyboardSettings *);
    long i = layoutIndexOf(inputSource);
    if (i < 0)
        return paramErr;
    SetFn set = (SetFn)skyLight("SLSSetKeyboardSettings");
    KeyboardSettings settings;
    currentSettings(&settings);
    strlcpy(settings.layout, kLayouts[i].layout, sizeof(settings.layout));
    strlcpy(settings.variant, kLayouts[i].variant, sizeof(settings.variant));
    if (set == NULL || set(&settings) != 0)
        return paramErr;
    Boolean enabled[kLayoutCount];
    enabledLayouts(enabled);
    enabled[i] = true;
    saveEnabled(enabled);
    CFDictionaryRef e = entryFor((size_t)i);
    CFArrayRef selected = CFArrayCreate(NULL, (const void **)&e, 1, &kCFTypeArrayCallBacks);
    CFPreferencesSetAppValue(CFSTR("AppleSelectedInputSources"), selected, kHIToolbox);
    CFStringRef sourceID = TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceID);
    CFPreferencesSetAppValue(CFSTR("AppleCurrentKeyboardLayoutInputSourceID"), sourceID, kHIToolbox);
    CFPreferencesAppSynchronize(kHIToolbox);
    CFRelease(selected);
    CFRelease(e);
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
        kTISNotifySelectedKeyboardInputSourceChanged, NULL, NULL, true);
    return noErr;
}

/* A keyboard layout is not deselected: another one is selected instead. */
OSStatus TISDeselectInputSource(TISInputSourceRef inputSource) {
    return paramErr;
}

OSStatus TISEnableInputSource(TISInputSourceRef inputSource) {
    long i = layoutIndexOf(inputSource);
    if (i < 0)
        return paramErr;
    Boolean enabled[kLayoutCount];
    enabledLayouts(enabled);
    enabled[i] = true;
    saveEnabled(enabled);
    return noErr;
}

/* Not the one in use (as on the reference, select another first). */
OSStatus TISDisableInputSource(TISInputSourceRef inputSource) {
    long i = layoutIndexOf(inputSource);
    if (i < 0 || i == currentLayoutIndex())
        return paramErr;
    Boolean enabled[kLayoutCount];
    enabledLayouts(enabled);
    enabled[i] = false;
    saveEnabled(enabled);
    return noErr;
}
