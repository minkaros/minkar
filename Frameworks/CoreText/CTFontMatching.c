/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Font descriptors and how they find fonts. A descriptor is a CF object
 * holding an attributes dictionary; matching asks fontconfig (the same
 * configuration the font manager registers fonts in) and describes each
 * face it lists by its PostScript name, family, style, file and traits.
 */

#include "CTFontDescriptor.h"
#include "CTFont.h"
#include "CTFontManager.h"
#include <CoreFoundation/CFRuntime.h>
#include <CoreGraphics/CGDataProvider.h>
#include <fontconfig/fontconfig.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

extern CFStringRef _CGFontCopyNameForID(CGFontRef font, uint16_t nameID);

/* A font made from memory keeps its bytes here (not a public key). */
static const CFStringRef kFontDataAttribute = CFSTR("_CTFontDataAttribute");

/* fontconfig is not safe to use from several threads at once. */
static pthread_mutex_t fcLock = PTHREAD_MUTEX_INITIALIZER;

struct __CTFontDescriptor {
    CFRuntimeBase base;
    CFDictionaryRef attributes;
};

static void descriptorFinalize(CFTypeRef cf) {
    struct __CTFontDescriptor *d = (struct __CTFontDescriptor *)cf;
    if (d->attributes != NULL)
        CFRelease(d->attributes);
}

static Boolean descriptorEqual(CFTypeRef a, CFTypeRef b) {
    return CFEqual(((struct __CTFontDescriptor *)a)->attributes, ((struct __CTFontDescriptor *)b)->attributes);
}

static CFHashCode descriptorHash(CFTypeRef cf) {
    return CFHash(((struct __CTFontDescriptor *)cf)->attributes);
}

static CFStringRef descriptorDescribe(CFTypeRef cf) {
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CTFontDescriptor %p> %@"), cf,
        ((struct __CTFontDescriptor *)cf)->attributes);
}

static const CFRuntimeClass descriptorClass = {
    0, "CTFontDescriptor", NULL, NULL, descriptorFinalize, descriptorEqual, descriptorHash, NULL, descriptorDescribe
};

CFTypeID CTFontDescriptorGetTypeID(void) {
    static CFTypeID typeID;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&descriptorClass); });
    return typeID;
}

static CTFontDescriptorRef createDescriptor(CFDictionaryRef attributes) {
    struct __CTFontDescriptor *d = (struct __CTFontDescriptor *)_CFRuntimeCreateInstance(NULL,
        CTFontDescriptorGetTypeID(), sizeof(struct __CTFontDescriptor) - sizeof(CFRuntimeBase), NULL);
    if (d == NULL)
        return NULL;
    d->attributes = attributes != NULL ? CFDictionaryCreateCopy(NULL, attributes)
        : CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return d;
}

static CFMutableDictionaryRef newMutable(void) {
    return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static void setDouble(CFMutableDictionaryRef d, CFStringRef key, double value) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberDoubleType, &value);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

static void setCString(CFMutableDictionaryRef d, CFStringRef key, const char *value) {
    CFStringRef s = value != NULL ? CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8) : NULL;
    if (s != NULL) {
        CFDictionarySetValue(d, key, s);
        CFRelease(s);
    }
}

CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes) {
    return createDescriptor(attributes);
}

CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef name, CGFloat size) {
    CFMutableDictionaryRef d = newMutable();
    if (name != NULL)
        CFDictionarySetValue(d, kCTFontNameAttribute, name);
    if (size > 0)
        setDouble(d, kCTFontSizeAttribute, size);
    CTFontDescriptorRef result = createDescriptor(d);
    CFRelease(d);
    return result;
}

static void addEntry(const void *key, const void *value, void *context) {
    CFDictionarySetValue((CFMutableDictionaryRef)context, key, value);
}

CTFontDescriptorRef CTFontDescriptorCreateCopyWithAttributes(CTFontDescriptorRef original, CFDictionaryRef attributes) {
    if (original == NULL)
        return NULL;
    CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, original->attributes);
    if (attributes != NULL)
        CFDictionaryApplyFunction(attributes, addEntry, d);
    CTFontDescriptorRef result = createDescriptor(d);
    CFRelease(d);
    return result;
}

CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor) {
    return descriptor != NULL ? CFDictionaryCreateCopy(NULL, descriptor->attributes) : NULL;
}

CFTypeRef CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
    if (descriptor == NULL || attribute == NULL)
        return NULL;
    CFTypeRef value = CFDictionaryGetValue(descriptor->attributes, attribute);
    return value != NULL ? CFRetain(value) : NULL;
}

/* Names are kept in one language: none is reported. */
CFTypeRef CTFontDescriptorCopyLocalizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute,
    CFStringRef *language) {
    if (language != NULL)
        *language = NULL;
    return CTFontDescriptorCopyAttribute(descriptor, attribute);
}

/* ---- Traits ---- */

/* CoreText's weight (-1..1) at each hundred of the OpenType scale. */
static const double weights[] = { -0.8, -0.6, -0.4, 0.0, 0.23, 0.3, 0.4, 0.56, 0.62 };

static double weightOfOpenType(double ot) {
    if (ot <= 100)
        return weights[0];
    if (ot >= 900)
        return weights[8];
    int i = (int)(ot / 100) - 1;
    double f = (ot - (i + 1) * 100) / 100;
    return weights[i] + (weights[i + 1] - weights[i]) * f;
}

static double openTypeOfWeight(double w) {
    if (w <= weights[0])
        return 100;
    if (w >= weights[8])
        return 900;
    for (int i = 0; i < 8; i++)
        if (w <= weights[i + 1])
            return (i + 1) * 100 + 100 * (w - weights[i]) / (weights[i + 1] - weights[i]);
    return 400;
}

static CFDictionaryRef copyTraits(FcPattern *p) {
    int weight = FC_WEIGHT_REGULAR, width = FC_WIDTH_NORMAL, slant = FC_SLANT_ROMAN, spacing = FC_PROPORTIONAL;
    FcPatternGetInteger(p, FC_WEIGHT, 0, &weight);
    FcPatternGetInteger(p, FC_WIDTH, 0, &width);
    FcPatternGetInteger(p, FC_SLANT, 0, &slant);
    FcPatternGetInteger(p, FC_SPACING, 0, &spacing);
    double ot = FcWeightToOpenTypeDouble(weight);
    uint32_t symbolic = 0;
    if (ot >= 600)
        symbolic |= kCTFontTraitBold;
    if (slant != FC_SLANT_ROMAN)
        symbolic |= kCTFontTraitItalic;
    if (width < FC_WIDTH_NORMAL)
        symbolic |= kCTFontTraitCondensed;
    if (width > FC_WIDTH_NORMAL)
        symbolic |= kCTFontTraitExpanded;
    if (spacing == FC_MONO)
        symbolic |= kCTFontTraitMonoSpace;
    CFMutableDictionaryRef traits = newMutable();
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &symbolic);
    CFDictionarySetValue(traits, kCTFontSymbolicTrait, n);
    CFRelease(n);
    setDouble(traits, kCTFontWeightTrait, weightOfOpenType(ot));
    double w = (width - FC_WIDTH_NORMAL) / 100.0;
    setDouble(traits, kCTFontWidthTrait, w < -1 ? -1 : w > 1 ? 1 : w);
    /* Slant: 1.0 is 180 degrees; a usual italic leans about 12. */
    setDouble(traits, kCTFontSlantTrait, slant == FC_SLANT_ROMAN ? 0 : 12.0 / 180.0);
    return traits;
}

static uint32_t symbolicTraits(CFDictionaryRef traits) {
    uint32_t value = 0;
    CFNumberRef n = traits != NULL ? CFDictionaryGetValue(traits, kCTFontSymbolicTrait) : NULL;
    if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
        CFNumberGetValue(n, kCFNumberSInt32Type, &value);
    return value;
}

static bool doubleIn(CFDictionaryRef d, CFStringRef key, double *value) {
    CFNumberRef n = d != NULL ? CFDictionaryGetValue(d, key) : NULL;
    return n != NULL && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberDoubleType, value);
}

/* ---- Descriptors and fontconfig patterns ---- */

static CTFontDescriptorRef descriptorFromPattern(FcPattern *p, CFNumberRef size) {
    CFMutableDictionaryRef d = newMutable();
    FcChar8 *s, *style = NULL, *family = NULL;
    if (FcPatternGetString(p, FC_POSTSCRIPT_NAME, 0, &s) == FcResultMatch)
        setCString(d, kCTFontNameAttribute, (const char *)s);
    if (FcPatternGetString(p, FC_FAMILY, 0, &family) == FcResultMatch)
        setCString(d, kCTFontFamilyNameAttribute, (const char *)family);
    if (FcPatternGetString(p, FC_STYLE, 0, &style) == FcResultMatch)
        setCString(d, kCTFontStyleNameAttribute, (const char *)style);
    else
        CFDictionarySetValue(d, kCTFontStyleNameAttribute, CFSTR("Regular"));
    if (FcPatternGetString(p, FC_FULLNAME, 0, &s) == FcResultMatch)
        setCString(d, kCTFontDisplayNameAttribute, (const char *)s);
    else if (family != NULL)
        setCString(d, kCTFontDisplayNameAttribute, (const char *)family);
    if (FcPatternGetString(p, FC_FILE, 0, &s) == FcResultMatch) {
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, s, strlen((const char *)s), false);
        if (url != NULL) {
            CFDictionarySetValue(d, kCTFontURLAttribute, url);
            CFRelease(url);
        }
    }
    if (FcPatternGetString(p, FC_FONTFORMAT, 0, &s) == FcResultMatch) {
        int format = !strcmp((const char *)s, "TrueType") ? kCTFontFormatTrueType
            : !strcmp((const char *)s, "CFF") ? kCTFontFormatOpenTypePostScript
            : !strcmp((const char *)s, "Type 1") ? kCTFontFormatPostScript
            : (!strcmp((const char *)s, "PCF") || !strcmp((const char *)s, "BDF")) ? kCTFontFormatBitmap
            : kCTFontFormatUnrecognized;
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &format);
        CFDictionarySetValue(d, kCTFontFormatAttribute, n);
        CFRelease(n);
    }
    CFDictionaryRef traits = copyTraits(p);
    CFDictionarySetValue(d, kCTFontTraitsAttribute, traits);
    CFRelease(traits);
    if (size != NULL)
        CFDictionarySetValue(d, kCTFontSizeAttribute, size);
    CTFontDescriptorRef result = createDescriptor(d);
    CFRelease(d);
    return result;
}

static bool addString(FcPattern *p, const char *object, CFTypeRef value) {
    char buffer[1024];
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID() ||
        !CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return false;
    FcPatternAddString(p, object, (const FcChar8 *)buffer);
    return true;
}

/* The pattern of a descriptor's names, file and languages: what a list
   must have exactly. The traits are compared afterwards. */
static FcPattern *patternFor(CFDictionaryRef attributes) {
    FcPattern *p = FcPatternCreate();
    addString(p, FC_POSTSCRIPT_NAME, CFDictionaryGetValue(attributes, kCTFontNameAttribute));
    addString(p, FC_FAMILY, CFDictionaryGetValue(attributes, kCTFontFamilyNameAttribute));
    addString(p, FC_STYLE, CFDictionaryGetValue(attributes, kCTFontStyleNameAttribute));
    CFURLRef url = CFDictionaryGetValue(attributes, kCTFontURLAttribute);
    char path[PATH_MAX];
    if (url != NULL && CFGetTypeID(url) == CFURLGetTypeID() &&
        CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path)))
        FcPatternAddString(p, FC_FILE, (const FcChar8 *)path);
    CFArrayRef languages = CFDictionaryGetValue(attributes, kCTFontLanguagesAttribute);
    if (languages != NULL && CFGetTypeID(languages) == CFArrayGetTypeID()) {
        FcLangSet *set = FcLangSetCreate();
        for (CFIndex i = 0; i < CFArrayGetCount(languages); i++) {
            char lang[64];
            CFStringRef l = CFArrayGetValueAtIndex(languages, i);
            if (CFGetTypeID(l) == CFStringGetTypeID() && CFStringGetCString(l, lang, sizeof(lang), kCFStringEncodingUTF8))
                FcLangSetAdd(set, (const FcChar8 *)lang);
        }
        FcPatternAddLangSet(p, FC_LANG, set);
        FcLangSetDestroy(set);
    }
    return p;
}

/* How far a face is from the traits asked for; -1 when it lacks a
   symbolic trait that was asked for. */
static double traitDistance(CFDictionaryRef wanted, CTFontDescriptorRef face) {
    if (wanted == NULL)
        return 0;
    CFDictionaryRef have = CFDictionaryGetValue(face->attributes, kCTFontTraitsAttribute);
    uint32_t mask = kCTFontTraitBold | kCTFontTraitItalic | kCTFontTraitMonoSpace | kCTFontTraitCondensed | kCTFontTraitExpanded;
    uint32_t want = symbolicTraits(wanted) & mask;
    if ((symbolicTraits(have) & want) != want)
        return -1;
    double distance = 0, a, b;
    if (doubleIn(wanted, kCTFontWeightTrait, &a) && doubleIn(have, kCTFontWeightTrait, &b))
        distance += (a - b) * (a - b) * 4;
    if (doubleIn(wanted, kCTFontWidthTrait, &a) && doubleIn(have, kCTFontWidthTrait, &b))
        distance += (a - b) * (a - b);
    if (doubleIn(wanted, kCTFontSlantTrait, &a) && doubleIn(have, kCTFontSlantTrait, &b))
        distance += (a - b) * (a - b);
    /* Unasked bold or italic count against a face. */
    distance += __builtin_popcount(symbolicTraits(have) & ~want & (kCTFontTraitBold | kCTFontTraitItalic)) * 0.5;
    return distance;
}

static CFArrayRef copyListed(CTFontDescriptorRef descriptor) {
    CFNumberRef size = CFDictionaryGetValue(descriptor->attributes, kCTFontSizeAttribute);
    FcConfig *config = _CTFontManagerSharedFontConfig();
    FcPattern *p = patternFor(descriptor->attributes);
    FcObjectSet *objects = FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FAMILY, FC_STYLE, FC_FULLNAME, FC_FILE,
        FC_FONTFORMAT, FC_WEIGHT, FC_WIDTH, FC_SLANT, FC_SPACING, FC_INDEX, (char *)0);
    pthread_mutex_lock(&fcLock);
    FcFontSet *set = FcFontList(config, p, objects);
    pthread_mutex_unlock(&fcLock);
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFMutableSetRef seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    for (int i = 0; set != NULL && i < set->nfont; i++) {
        CTFontDescriptorRef d = descriptorFromPattern(set->fonts[i], size);
        CFTypeRef name = CFDictionaryGetValue(d->attributes, kCTFontNameAttribute);
        /* A face is listed once (fontconfig lists each of its languages'
           names), and only with a PostScript name and a family: a face
           without them cannot be opened by name. */
        if (name != NULL && CFDictionaryGetValue(d->attributes, kCTFontFamilyNameAttribute) != NULL &&
            !CFSetContainsValue(seen, name)) {
            CFSetAddValue(seen, name);
            CFArrayAppendValue(list, d);
        }
        CFRelease(d);
    }
    CFRelease(seen);
    if (set != NULL)
        FcFontSetDestroy(set);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(p);
    return list;
}

CFArrayRef CTFontDescriptorCreateMatchingFontDescriptors(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes) {
    if (descriptor == NULL)
        return NULL;
    CFArrayRef listed = copyListed(descriptor);
    CFDictionaryRef wanted = CFDictionaryGetValue(descriptor->attributes, kCTFontTraitsAttribute);
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < CFArrayGetCount(listed); i++) {
        CTFontDescriptorRef d = CFArrayGetValueAtIndex(listed, i);
        if (traitDistance(wanted, d) >= 0)
            CFArrayAppendValue(result, d);
    }
    CFRelease(listed);
    if (CFArrayGetCount(result) == 0) {
        CFRelease(result);
        return NULL;
    }
    return result;
}

CTFontDescriptorRef CTFontDescriptorCreateMatchingFontDescriptor(CTFontDescriptorRef descriptor, CFSetRef mandatoryAttributes) {
    if (descriptor == NULL)
        return NULL;
    CFArrayRef listed = copyListed(descriptor);
    CFDictionaryRef wanted = CFDictionaryGetValue(descriptor->attributes, kCTFontTraitsAttribute);
    CTFontDescriptorRef best = NULL;
    double bestDistance = 0;
    for (CFIndex i = 0; i < CFArrayGetCount(listed); i++) {
        CTFontDescriptorRef d = CFArrayGetValueAtIndex(listed, i);
        double distance = traitDistance(wanted, d);
        if (distance >= 0 && (best == NULL || distance < bestDistance)) {
            best = d;
            bestDistance = distance;
        }
    }
    if (best != NULL)
        CFRetain(best);
    CFRelease(listed);
    if (best != NULL || mandatoryAttributes != NULL)
        return best;

    /* Nothing has those names: fontconfig's closest face. */
    FcConfig *config = _CTFontManagerSharedFontConfig();
    FcPattern *p = patternFor(descriptor->attributes);
    if (wanted != NULL) {
        double w;
        uint32_t symbolic = symbolicTraits(wanted);
        if (doubleIn(wanted, kCTFontWeightTrait, &w))
            FcPatternAddInteger(p, FC_WEIGHT, FcWeightFromOpenTypeDouble(openTypeOfWeight(w)));
        else if (symbolic & kCTFontTraitBold)
            FcPatternAddInteger(p, FC_WEIGHT, FC_WEIGHT_BOLD);
        if (symbolic & kCTFontTraitItalic)
            FcPatternAddInteger(p, FC_SLANT, FC_SLANT_ITALIC);
        if (symbolic & kCTFontTraitMonoSpace)
            FcPatternAddInteger(p, FC_SPACING, FC_MONO);
    }
    FcResult r;
    pthread_mutex_lock(&fcLock);
    FcConfigSubstitute(config, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    FcPattern *match = FcFontMatch(config, p, &r);
    pthread_mutex_unlock(&fcLock);
    FcPatternDestroy(p);
    if (match == NULL)
        return NULL;
    best = descriptorFromPattern(match, CFDictionaryGetValue(descriptor->attributes, kCTFontSizeAttribute));
    FcPatternDestroy(match);
    return best;
}

/* ---- Fonts from descriptors, and a font's descriptor ---- */

static CTFontRef applyVariation(CTFontRef font, CFDictionaryRef variation) {
    if (font == NULL || variation == NULL || CFGetTypeID(variation) != CFDictionaryGetTypeID() ||
        CFDictionaryGetCount(variation) == 0)
        return font;
    CGFontRef cg = CTFontCopyGraphicsFont(font, NULL);
    CGFontRef instance = CGFontCreateCopyWithVariations(cg, variation);
    CTFontRef result = font;
    if (instance != NULL && instance != cg) {
        result = CTFontCreateWithGraphicsFont(instance, CTFontGetSize(font), NULL, NULL);
        CFRelease(font);
    }
    if (instance != NULL)
        CGFontRelease(instance);
    CGFontRelease(cg);
    return result;
}

CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef descriptor, CGFloat size, const CGAffineTransform *matrix) {
    if (descriptor == NULL)
        return NULL;
    CFDictionaryRef attributes = descriptor->attributes;
    double wantedSize;
    if (size <= 0)
        size = doubleIn(attributes, kCTFontSizeAttribute, &wantedSize) && wantedSize > 0 ? wantedSize : 12.0;

    CTFontRef font = NULL;
    CFDataRef data = CFDictionaryGetValue(attributes, kFontDataAttribute);
    if (data != NULL) {
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGFontRef cg = provider != NULL ? CGFontCreateWithDataProvider(provider) : NULL;
        if (provider != NULL)
            CGDataProviderRelease(provider);
        if (cg != NULL) {
            font = CTFontCreateWithGraphicsFont(cg, size, NULL, NULL);
            CGFontRelease(cg);
        }
    } else {
        CFStringRef name = CFDictionaryGetValue(attributes, kCTFontNameAttribute);
        CTFontDescriptorRef match = NULL;
        if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID()) {
            match = CTFontDescriptorCreateMatchingFontDescriptor(descriptor, NULL);
            name = match != NULL ? CFDictionaryGetValue(match->attributes, kCTFontNameAttribute) : NULL;
        }
        font = CTFontCreateWithName(name, size, matrix);
        if (match != NULL)
            CFRelease(match);
    }
    return applyVariation(font, CFDictionaryGetValue(attributes, kCTFontVariationAttribute));
}

static FcPattern *copyPatternOfFont(CFStringRef postScriptName) {
    char name[512];
    if (postScriptName == NULL || !CFStringGetCString(postScriptName, name, sizeof(name), kCFStringEncodingUTF8))
        return NULL;
    FcConfig *config = _CTFontManagerSharedFontConfig();
    FcPattern *p = FcPatternCreate();
    FcPatternAddString(p, FC_POSTSCRIPT_NAME, (const FcChar8 *)name);
    FcObjectSet *objects = FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FAMILY, FC_STYLE, FC_FULLNAME, FC_FILE,
        FC_FONTFORMAT, FC_WEIGHT, FC_WIDTH, FC_SLANT, FC_SPACING, (char *)0);
    pthread_mutex_lock(&fcLock);
    FcFontSet *set = FcFontList(config, p, objects);
    pthread_mutex_unlock(&fcLock);
    FcPattern *result = NULL;
    if (set != NULL && set->nfont > 0) {
        result = set->fonts[0];
        FcPatternReference(result);
    }
    if (set != NULL)
        FcFontSetDestroy(set);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(p);
    return result;
}

CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font) {
    if (font == NULL)
        return NULL;
    CGFontRef cg = CTFontCopyGraphicsFont(font, NULL);
    CFStringRef psName = CGFontCopyPostScriptName(cg);
    double size = CTFontGetSize(font);
    CFNumberRef sizeNumber = CFNumberCreate(NULL, kCFNumberDoubleType, &size);
    FcPattern *p = copyPatternOfFont(psName);
    CTFontDescriptorRef result;
    if (p != NULL) {
        result = descriptorFromPattern(p, sizeNumber);
        FcPatternDestroy(p);
    } else {
        /* A font fontconfig does not know (made from data): its own names. */
        CFMutableDictionaryRef d = newMutable();
        CFStringRef s;
        if (psName != NULL)
            CFDictionarySetValue(d, kCTFontNameAttribute, psName);
        if ((s = _CGFontCopyNameForID(cg, 16)) != NULL || (s = _CGFontCopyNameForID(cg, 1)) != NULL) {
            CFDictionarySetValue(d, kCTFontFamilyNameAttribute, s);
            CFRelease(s);
        }
        if ((s = _CGFontCopyNameForID(cg, 17)) != NULL || (s = _CGFontCopyNameForID(cg, 2)) != NULL) {
            CFDictionarySetValue(d, kCTFontStyleNameAttribute, s);
            CFRelease(s);
        }
        if ((s = _CGFontCopyNameForID(cg, 4)) != NULL) {
            CFDictionarySetValue(d, kCTFontDisplayNameAttribute, s);
            CFRelease(s);
        }
        CFMutableDictionaryRef traits = newMutable();
        uint32_t symbolic = CTFontGetSymbolicTraits(font);
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &symbolic);
        CFDictionarySetValue(traits, kCTFontSymbolicTrait, n);
        CFRelease(n);
        setDouble(traits, kCTFontWeightTrait, (symbolic & kCTFontTraitBold) ? 0.4 : 0.0);
        setDouble(traits, kCTFontWidthTrait, 0);
        setDouble(traits, kCTFontSlantTrait, (symbolic & kCTFontTraitItalic) ? 12.0 / 180.0 : 0);
        CFDictionarySetValue(d, kCTFontTraitsAttribute, traits);
        CFRelease(traits);
        CFDictionarySetValue(d, kCTFontSizeAttribute, sizeNumber);
        result = createDescriptor(d);
        CFRelease(d);
    }
    CFDictionaryRef variation = CGFontCopyVariations(cg);
    if (variation != NULL) {
        CFDictionaryRef tagged = CTFontCopyVariation(font);
        if (tagged != NULL) {
            CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, result->attributes);
            CFDictionarySetValue(d, kCTFontVariationAttribute, tagged);
            CFRelease(tagged);
            CFRelease(result);
            result = createDescriptor(d);
            CFRelease(d);
        }
        CFRelease(variation);
    }
    CFRelease(sizeNumber);
    if (psName != NULL)
        CFRelease(psName);
    CGFontRelease(cg);
    return result;
}

CFTypeRef CTFontCopyAttribute(CTFontRef font, CFStringRef attribute) {
    CTFontDescriptorRef d = CTFontCopyFontDescriptor(font);
    CFTypeRef value = CTFontDescriptorCopyAttribute(d, attribute);
    if (d != NULL)
        CFRelease(d);
    return value;
}

/* Fallback without a language: every face once, in the order the
   configuration prefers families, and what was found for each character.
   fontconfig ranks a face that has the character before one that lacks it
   and only then by family, so the first face in this order that has it is
   the one a sort by that character would give. Sorting all faces for each
   character (a browser asks for every run of text) cost milliseconds each.
   Rebuilt when faces are added or removed. Under fcLock. */
static int fallbackGeneration = -1;
static CFMutableDictionaryRef fallbackFound;	/* character + 1 -> name or kCFNull */

/* The faces a script is best drawn in, before the configuration's order,
   which knows no script and took a fixed-pitch Korean face for Japanese
   and DejaVu for Arabic. Japanese first for Han, which has no language
   here (an app that knows it names its font). */
static const char *const cjkFamilies[] = { "Noto Sans CJK JP", "Noto Sans CJK SC", "Noto Sans CJK TC", "Noto Sans CJK KR", NULL };
static const char *const hangulFamilies[] = { "Noto Sans CJK KR", NULL };
static const char *const arabicFamilies[] = { "Noto Sans Arabic", NULL };

static const char *const *preferredFamilies(UTF32Char c) {
    if ((c >= 0x1100 && c <= 0x11FF) || (c >= 0x3130 && c <= 0x318F) || (c >= 0xA960 && c <= 0xA97F) ||
        (c >= 0xAC00 && c <= 0xD7FF))
        return hangulFamilies;
    if ((c >= 0x2E80 && c <= 0x2FDF) || (c >= 0x3000 && c <= 0x30FF) || (c >= 0x3190 && c <= 0x9FFF) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF) || (c >= 0x20000 && c <= 0x3FFFF))
        return cjkFamilies;
    if ((c >= 0x0600 && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) || (c >= 0x0870 && c <= 0x08FF) ||
        (c >= 0xFB50 && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF))
        return arabicFamilies;
    return NULL;
}

static int inFamily(FcPattern *face, const char *wanted) {
    FcChar8 *family;
    for (int n = 0; FcPatternGetString(face, FC_FAMILY, n, &family) == FcResultMatch; n++)
        if (strcmp((const char *)family, wanted) == 0)
            return 1;
    return 0;
}

static FcFontSet *fallbackFaces;

/* The first face, in the configuration's order, that has the character
   (and is of family, when one is given). */
static CFStringRef copyFaceWithCharacter(UTF32Char character, const char *family) {
    for (int i = 0; fallbackFaces != NULL && i < fallbackFaces->nfont; i++) {
        FcCharSet *has;
        FcChar8 *name;
        if (family != NULL && !inFamily(fallbackFaces->fonts[i], family))
            continue;
        if (FcPatternGetCharSet(fallbackFaces->fonts[i], FC_CHARSET, 0, &has) == FcResultMatch &&
            FcCharSetHasChar(has, character) &&
            FcPatternGetString(fallbackFaces->fonts[i], FC_POSTSCRIPT_NAME, 0, &name) == FcResultMatch)
            return CFStringCreateWithCString(NULL, (const char *)name, kCFStringEncodingUTF8);
    }
    return NULL;
}

static int fontCount(FcConfig *config) {
    FcFontSet *system = FcConfigGetFonts(config, FcSetSystem);
    FcFontSet *application = FcConfigGetFonts(config, FcSetApplication);
    return (system != NULL ? system->nfont : 0) + (application != NULL ? application->nfont : 0);
}

static CFStringRef copyFallbackName(FcConfig *config, UTF32Char character) {
    int generation = fontCount(config);
    if (fallbackFaces == NULL || generation != fallbackGeneration) {
        if (fallbackFaces != NULL)
            FcFontSetDestroy(fallbackFaces);
        FcPattern *p = FcPatternCreate();
        FcResult r;
        FcConfigSubstitute(config, p, FcMatchPattern);
        FcDefaultSubstitute(p);
        fallbackFaces = FcFontSort(config, p, FcFalse, NULL, &r);
        FcPatternDestroy(p);
        fallbackGeneration = generation;
        if (fallbackFound == NULL)
            fallbackFound = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryRemoveAllValues(fallbackFound);
    }
    const void *key = (const void *)(uintptr_t)(character + 1);
    CFTypeRef found = CFDictionaryGetValue(fallbackFound, key);
    if (found != NULL)
        return found == kCFNull ? NULL : CFRetain(found);

    CFStringRef result = NULL;
    const char *const *preferred = preferredFamilies(character);
    for (int f = 0; preferred != NULL && preferred[f] != NULL && result == NULL; f++)
        result = copyFaceWithCharacter(character, preferred[f]);
    if (result == NULL)
        result = copyFaceWithCharacter(character, NULL);
    if (CFDictionaryGetCount(fallbackFound) >= 65536)
        CFDictionaryRemoveAllValues(fallbackFound);
    CFDictionarySetValue(fallbackFound, key, result != NULL ? (CFTypeRef)result : kCFNull);
    return result;
}

/* A face that has the character, for fallback: fontconfig's sort by the
   character set, as the configuration orders families. */
CFStringRef _CTFontCopyPostScriptNameForCharacter(UTF32Char character, CFStringRef language) {
    FcConfig *config = _CTFontManagerSharedFontConfig();
    if (language == NULL) {
        pthread_mutex_lock(&fcLock);
        CFStringRef name = copyFallbackName(config, character);
        pthread_mutex_unlock(&fcLock);
        return name;
    }
    FcPattern *p = FcPatternCreate();
    FcCharSet *cs = FcCharSetCreate();
    FcCharSetAddChar(cs, character);
    FcPatternAddCharSet(p, FC_CHARSET, cs);
    char lang[64];
    if (language != NULL && CFStringGetCString(language, lang, sizeof(lang), kCFStringEncodingUTF8))
        FcPatternAddString(p, FC_LANG, (const FcChar8 *)lang);
    FcResult r;
    CFStringRef result = NULL;
    pthread_mutex_lock(&fcLock);
    FcConfigSubstitute(config, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    FcFontSet *set = FcFontSort(config, p, FcTrue, NULL, &r);
    pthread_mutex_unlock(&fcLock);
    for (int i = 0; set != NULL && i < set->nfont && result == NULL; i++) {
        FcCharSet *has;
        FcChar8 *name;
        if (FcPatternGetCharSet(set->fonts[i], FC_CHARSET, 0, &has) == FcResultMatch &&
            FcCharSetHasChar(has, character) &&
            FcPatternGetString(set->fonts[i], FC_POSTSCRIPT_NAME, 0, &name) == FcResultMatch)
            result = CFStringCreateWithCString(NULL, (const char *)name, kCFStringEncodingUTF8);
    }
    if (set != NULL)
        FcFontSetDestroy(set);
    FcCharSetDestroy(cs);
    FcPatternDestroy(p);
    return result;
}

/* ---- Descriptors of files and data ---- */

CFArrayRef CTFontManagerCreateFontDescriptorsFromURL(CFURLRef fileURL) {
    char path[PATH_MAX];
    if (fileURL == NULL || !CFURLGetFileSystemRepresentation(fileURL, true, (UInt8 *)path, sizeof(path)))
        return NULL;
    FcFontSet *set = FcFontSetCreate();
    int count = 0;
    pthread_mutex_lock(&fcLock);
    FcFreeTypeQueryAll((const FcChar8 *)path, (unsigned int)-1, NULL, &count, set);
    pthread_mutex_unlock(&fcLock);
    CFMutableArrayRef result = NULL;
    for (int i = 0; i < set->nfont; i++) {
        if (result == NULL)
            result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CTFontDescriptorRef d = descriptorFromPattern(set->fonts[i], NULL);
        CFArrayAppendValue(result, d);
        CFRelease(d);
    }
    FcFontSetDestroy(set);
    return result;
}

CTFontDescriptorRef CTFontManagerCreateFontDescriptorFromData(CFDataRef data) {
    if (data == NULL || CFDataGetLength(data) == 0)
        return NULL;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGFontRef cg = provider != NULL ? CGFontCreateWithDataProvider(provider) : NULL;
    if (provider != NULL)
        CGDataProviderRelease(provider);
    if (cg == NULL)
        return NULL;
    CFMutableDictionaryRef d = newMutable();
    CFStringRef s;
    if ((s = CGFontCopyPostScriptName(cg)) != NULL) {
        CFDictionarySetValue(d, kCTFontNameAttribute, s);
        CFRelease(s);
    }
    if ((s = _CGFontCopyNameForID(cg, 16)) != NULL || (s = _CGFontCopyNameForID(cg, 1)) != NULL) {
        CFDictionarySetValue(d, kCTFontFamilyNameAttribute, s);
        CFRelease(s);
    }
    if ((s = _CGFontCopyNameForID(cg, 17)) != NULL || (s = _CGFontCopyNameForID(cg, 2)) != NULL) {
        CFDictionarySetValue(d, kCTFontStyleNameAttribute, s);
        CFRelease(s);
    }
    if ((s = _CGFontCopyNameForID(cg, 4)) != NULL) {
        CFDictionarySetValue(d, kCTFontDisplayNameAttribute, s);
        CFRelease(s);
    }
    CFDictionarySetValue(d, kFontDataAttribute, data);
    CGFontRelease(cg);
    CTFontDescriptorRef result = createDescriptor(d);
    CFRelease(d);
    return result;
}
