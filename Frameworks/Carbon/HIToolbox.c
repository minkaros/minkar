/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "HIToolbox.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct Mode {
    OptionBits options;
    struct Mode *below;
} Mode;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Mode *sTop;

void *PushSymbolicHotKeyMode(OptionBits inOptions) {
    Mode *mode = calloc(1, sizeof(Mode));
    if (mode == NULL)
        return NULL;
    pthread_mutex_lock(&sLock);
    mode->options = inOptions;
    mode->below = sTop;
    sTop = mode;
    pthread_mutex_unlock(&sLock);
    return mode;
}

void PopSymbolicHotKeyMode(void *inToken) {
    pthread_mutex_lock(&sLock);
    for (Mode **link = &sTop; *link != NULL; link = &(*link)->below)
        if (*link == inToken) {
            Mode *mode = *link;
            *link = mode->below;
            free(mode);
            break;
        }
    pthread_mutex_unlock(&sLock);
}

OptionBits GetSymbolicHotKeyMode(void) {
    OptionBits options;
    pthread_mutex_lock(&sLock);
    options = (sTop != NULL) ? sTop->options : kHIHotKeyModeAllEnabled;
    pthread_mutex_unlock(&sLock);
    return options;
}

/* -- Keyboard state -- */

#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#include <stdlib.h>
#include <string.h>

static UInt32 carbonModifiers(CGEventFlags flags) {
    UInt32 m = 0;
    if (flags & kCGEventFlagMaskCommand) m |= cmdKey;
    if (flags & kCGEventFlagMaskShift) m |= shiftKey;
    if (flags & kCGEventFlagMaskAlphaShift) m |= alphaLock;
    if (flags & kCGEventFlagMaskAlternate) m |= optionKey;
    if (flags & kCGEventFlagMaskControl) m |= controlKey;
    return m;
}

UInt32 GetCurrentEventKeyModifiers(void) {
    return carbonModifiers(CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState));
}

UInt32 GetCurrentKeyModifiers(void) {
    return GetCurrentEventKeyModifiers();
}

static pthread_mutex_t secureLock = PTHREAD_MUTEX_INITIALIZER;
static int secureCount;

OSStatus EnableSecureEventInput(void) {
    pthread_mutex_lock(&secureLock);
    secureCount++;
    pthread_mutex_unlock(&secureLock);
    return noErr;
}

OSStatus DisableSecureEventInput(void) {
    pthread_mutex_lock(&secureLock);
    if (secureCount > 0)
        secureCount--;
    pthread_mutex_unlock(&secureLock);
    return noErr;
}

Boolean IsSecureEventInputEnabled(void) {
    pthread_mutex_lock(&secureLock);
    Boolean on = secureCount > 0;
    pthread_mutex_unlock(&secureLock);
    return on;
}

PhysicalKeyboardLayoutType KBGetLayoutType(SInt16 iKeyboardType) {
    return kKeyboardANSI;
}

/* An ANSI keyboard's type number in the Keyboard Manager's numbering. */
UInt8 LMGetKbdType(void) {
    return 40;
}

/* -- Text services documents -- */

struct OpaqueTSMDocumentID {
    CFMutableDictionaryRef properties;	/* tag (as a number) -> CFData */
};

TSMDocumentID TSMGetActiveDocument(void) {
    static struct OpaqueTSMDocumentID document;
    pthread_mutex_lock(&secureLock);
    if (document.properties == NULL)
        document.properties = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    pthread_mutex_unlock(&secureLock);
    return &document;
}

static CFNumberRef tagKey(TSMDocumentPropertyTag tag) {
    SInt64 v = tag;
    return CFNumberCreate(NULL, kCFNumberSInt64Type, &v);
}

OSStatus TSMSetDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag,
    UInt32 propertySize, void *propertyData) {
    if (docID == NULL || docID->properties == NULL)
        return tsmInvalidDocumentErr;
    CFNumberRef key = tagKey(propertyTag);
    CFDataRef data = CFDataCreate(NULL, propertyData, propertyData != NULL ? propertySize : 0);
    pthread_mutex_lock(&secureLock);
    CFDictionarySetValue(docID->properties, key, data);
    pthread_mutex_unlock(&secureLock);
    CFRelease(data);
    CFRelease(key);
    return noErr;
}

OSStatus TSMGetDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag,
    UInt32 bufferSize, UInt32 *actualSize, void *propertyBuffer) {
    if (docID == NULL || docID->properties == NULL)
        return tsmInvalidDocumentErr;
    CFNumberRef key = tagKey(propertyTag);
    OSStatus status = noErr;
    pthread_mutex_lock(&secureLock);
    CFDataRef data = CFDictionaryGetValue(docID->properties, key);
    if (data == NULL) {
        status = tsmDocPropertyNotFoundErr;
    } else {
        UInt32 size = (UInt32)CFDataGetLength(data);
        if (actualSize != NULL)
            *actualSize = size;
        if (propertyBuffer != NULL) {
            if (bufferSize < size)
                status = tsmDocPropertyBufferTooSmallErr;
            else
                memcpy(propertyBuffer, CFDataGetBytePtr(data), size);
        }
    }
    pthread_mutex_unlock(&secureLock);
    CFRelease(key);
    return status;
}

OSStatus TSMRemoveDocumentProperty(TSMDocumentID docID, TSMDocumentPropertyTag propertyTag) {
    if (docID == NULL || docID->properties == NULL)
        return tsmInvalidDocumentErr;
    CFNumberRef key = tagKey(propertyTag);
    pthread_mutex_lock(&secureLock);
    Boolean had = CFDictionaryContainsKey(docID->properties, key);
    CFDictionaryRemoveValue(docID->properties, key);
    pthread_mutex_unlock(&secureLock);
    CFRelease(key);
    return had ? noErr : tsmDocPropertyNotFoundErr;
}

/* -- Appearance -- */

OSStatus GetThemeMetric(ThemeMetric inMetric, SInt32 *outMetric) {
    static const SInt32 metrics[] = {
        [kThemeMetricScrollBarWidth] = 15,
        [kThemeMetricSmallScrollBarWidth] = 11,
        [kThemeMetricCheckBoxHeight] = 16,
        [kThemeMetricRadioButtonHeight] = 16,
        [kThemeMetricEditTextWhitespace] = 2,
        [kThemeMetricEditTextFrameOutset] = 1,
        [kThemeMetricListBoxFrameOutset] = 1,
        [kThemeMetricFocusRectOutset] = 3,
    };
    if (outMetric == NULL)
        return paramErr;
    if (inMetric >= sizeof(metrics) / sizeof(metrics[0])) {
        *outMetric = 0;
        return themeInvalidMetricErr;
    }
    *outMetric = metrics[inMetric];
    return noErr;
}

/* A rounded bezel, lighter when pressed; a check when the value is on. */
OSStatus HIThemeDrawButton(const CGRect *inBounds, const HIThemeButtonDrawInfo *inDrawInfo,
    CGContextRef inContext, HIThemeOrientation inOrientation, CGRect *outLabelRect) {
    if (inBounds == NULL || inDrawInfo == NULL || inContext == NULL)
        return paramErr;
    CGRect r = CGRectInset(*inBounds, 0.5, 0.5);
    CGFloat radius = r.size.height < 12 ? r.size.height / 2 : 5;
    Boolean pressed = inDrawInfo->state == kThemeStatePressed;
    Boolean active = inDrawInfo->state != kThemeStateInactive;
    CGContextSaveGState(inContext);
    CGPathRef path = CGPathCreateWithRoundedRect(r, radius, radius, NULL);
    CGFloat fill = pressed ? 0.82 : 0.97;
    CGContextSetRGBFillColor(inContext, fill, fill, fill, active ? 1.0 : 0.6);
    CGContextAddPath(inContext, path);
    CGContextFillPath(inContext);
    CGContextSetRGBStrokeColor(inContext, 0.65, 0.65, 0.65, active ? 1.0 : 0.5);
    CGContextSetLineWidth(inContext, 1.0);
    CGContextAddPath(inContext, path);
    CGContextStrokePath(inContext);
    CGPathRelease(path);
    if (inDrawInfo->value == kThemeButtonOn) {
        CGFloat x = CGRectGetMinX(r), y = CGRectGetMinY(r), w = r.size.width, h = r.size.height;
        CGContextSetRGBStrokeColor(inContext, 0.15, 0.35, 0.75, active ? 1.0 : 0.5);
        CGContextSetLineWidth(inContext, 2.0);
        CGContextMoveToPoint(inContext, x + w * 0.25, y + h * 0.5);
        CGContextAddLineToPoint(inContext, x + w * 0.45, y + h * 0.7);
        CGContextAddLineToPoint(inContext, x + w * 0.75, y + h * 0.3);
        CGContextStrokePath(inContext);
    } else if (inDrawInfo->value == kThemeButtonMixed) {
        CGContextSetRGBFillColor(inContext, 0.15, 0.35, 0.75, active ? 1.0 : 0.5);
        CGContextFillRect(inContext, CGRectMake(CGRectGetMinX(r) + r.size.width * 0.25,
            CGRectGetMidY(r) - 1, r.size.width * 0.5, 2));
    }
    CGContextRestoreGState(inContext);
    if (outLabelRect != NULL)
        *outLabelRect = CGRectInset(*inBounds, radius, 2);
    return noErr;
}
