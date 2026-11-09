/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFAttributedString on Foundation's NSAttributedString and
 * NSMutableAttributedString: each function sends the class or the string
 * its message (CF strings and dictionaries are NS ones).
 */

#include <CoreFoundation/CFAttributedString.h>
#include <CoreFoundation/CFRuntime.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <stddef.h>

typedef struct { unsigned long location, length; } NSRange_;

static id cls(const char *name) { return (id)objc_getClass(name); }
static SEL sel(const char *name) { return sel_registerName(name); }

static id send0(id obj, const char *name) { return ((id (*)(id, SEL))objc_msgSend)(obj, sel(name)); }
static id send1(id obj, const char *name, id a) { return ((id (*)(id, SEL, id))objc_msgSend)(obj, sel(name), a); }

static NSRange_ nsrange(CFRange r) { NSRange_ n = { (unsigned long)r.location, (unsigned long)r.length }; return n; }
static CFRange cfrange(NSRange_ r) { return CFRangeMake((CFIndex)r.location, (CFIndex)r.length); }

static CFTypeID attributedStringTypeID = 0;
static const CFRuntimeClass attributedStringClass = { 0, "CFAttributedString", NULL, NULL, NULL, NULL, NULL, NULL, NULL };

CFTypeID CFAttributedStringGetTypeID(void) {
    if (attributedStringTypeID == 0)
        attributedStringTypeID = _CFRuntimeRegisterClass(&attributedStringClass);
    return attributedStringTypeID;
}

CFAttributedStringRef CFAttributedStringCreate(CFAllocatorRef alloc, CFStringRef str, CFDictionaryRef attributes) {
    id obj = send0(cls("NSAttributedString"), "alloc");
    return (CFAttributedStringRef)((id (*)(id, SEL, id, id))objc_msgSend)(obj, sel("initWithString:attributes:"), (id)str, (id)attributes);
}

CFAttributedStringRef CFAttributedStringCreateWithSubstring(CFAllocatorRef alloc, CFAttributedStringRef aStr, CFRange range) {
    id sub = ((id (*)(id, SEL, NSRange_))objc_msgSend)((id)aStr, sel("attributedSubstringFromRange:"), nsrange(range));
    return (CFAttributedStringRef)send0(sub, "copy");
}

CFAttributedStringRef CFAttributedStringCreateCopy(CFAllocatorRef alloc, CFAttributedStringRef aStr) {
    return (CFAttributedStringRef)send0((id)aStr, "copy");
}

CFStringRef CFAttributedStringGetString(CFAttributedStringRef aStr) {
    return (CFStringRef)send0((id)aStr, "string");
}

CFIndex CFAttributedStringGetLength(CFAttributedStringRef aStr) {
    return (CFIndex)((unsigned long (*)(id, SEL))objc_msgSend)((id)aStr, sel("length"));
}

CFDictionaryRef CFAttributedStringGetAttributes(CFAttributedStringRef aStr, CFIndex loc, CFRange *effectiveRange) {
    NSRange_ r = { 0, 0 };
    id result = ((id (*)(id, SEL, unsigned long, NSRange_ *))objc_msgSend)((id)aStr, sel("attributesAtIndex:effectiveRange:"),
        (unsigned long)loc, effectiveRange ? &r : NULL);
    if (effectiveRange)
        *effectiveRange = cfrange(r);
    return (CFDictionaryRef)result;
}

CFTypeRef CFAttributedStringGetAttribute(CFAttributedStringRef aStr, CFIndex loc, CFStringRef attrName, CFRange *effectiveRange) {
    NSRange_ r = { 0, 0 };
    id result = ((id (*)(id, SEL, id, unsigned long, NSRange_ *))objc_msgSend)((id)aStr, sel("attribute:atIndex:effectiveRange:"),
        (id)attrName, (unsigned long)loc, effectiveRange ? &r : NULL);
    if (effectiveRange)
        *effectiveRange = cfrange(r);
    return (CFTypeRef)result;
}

CFDictionaryRef CFAttributedStringGetAttributesAndLongestEffectiveRange(CFAttributedStringRef aStr, CFIndex loc, CFRange inRange, CFRange *longestEffectiveRange) {
    NSRange_ r = { 0, 0 };
    id result = ((id (*)(id, SEL, unsigned long, NSRange_ *, NSRange_))objc_msgSend)((id)aStr,
        sel("attributesAtIndex:longestEffectiveRange:inRange:"), (unsigned long)loc, longestEffectiveRange ? &r : NULL, nsrange(inRange));
    if (longestEffectiveRange)
        *longestEffectiveRange = cfrange(r);
    return (CFDictionaryRef)result;
}

CFTypeRef CFAttributedStringGetAttributeAndLongestEffectiveRange(CFAttributedStringRef aStr, CFIndex loc, CFStringRef attrName, CFRange inRange, CFRange *longestEffectiveRange) {
    NSRange_ r = { 0, 0 };
    id result = ((id (*)(id, SEL, id, unsigned long, NSRange_ *, NSRange_))objc_msgSend)((id)aStr,
        sel("attribute:atIndex:longestEffectiveRange:inRange:"), (id)attrName, (unsigned long)loc,
        longestEffectiveRange ? &r : NULL, nsrange(inRange));
    if (longestEffectiveRange)
        *longestEffectiveRange = cfrange(r);
    return (CFTypeRef)result;
}

CFMutableAttributedStringRef CFAttributedStringCreateMutableCopy(CFAllocatorRef alloc, CFIndex maxLength, CFAttributedStringRef aStr) {
    return (CFMutableAttributedStringRef)send0((id)aStr, "mutableCopy");
}

CFMutableAttributedStringRef CFAttributedStringCreateMutable(CFAllocatorRef alloc, CFIndex maxLength) {
    return (CFMutableAttributedStringRef)send0(send0(cls("NSMutableAttributedString"), "alloc"), "init");
}

void CFAttributedStringReplaceString(CFMutableAttributedStringRef aStr, CFRange range, CFStringRef replacement) {
    ((void (*)(id, SEL, NSRange_, id))objc_msgSend)((id)aStr, sel("replaceCharactersInRange:withString:"), nsrange(range), (id)replacement);
}

CFMutableStringRef CFAttributedStringGetMutableString(CFMutableAttributedStringRef aStr) {
    return (CFMutableStringRef)send0((id)aStr, "mutableString");
}

void CFAttributedStringSetAttributes(CFMutableAttributedStringRef aStr, CFRange range, CFDictionaryRef replacement, Boolean clearOtherAttributes) {
    if (clearOtherAttributes)
        ((void (*)(id, SEL, id, NSRange_))objc_msgSend)((id)aStr, sel("setAttributes:range:"), (id)replacement, nsrange(range));
    else
        ((void (*)(id, SEL, id, NSRange_))objc_msgSend)((id)aStr, sel("addAttributes:range:"), (id)replacement, nsrange(range));
}

void CFAttributedStringSetAttribute(CFMutableAttributedStringRef aStr, CFRange range, CFStringRef attrName, CFTypeRef value) {
    ((void (*)(id, SEL, id, id, NSRange_))objc_msgSend)((id)aStr, sel("addAttribute:value:range:"), (id)attrName, (id)value, nsrange(range));
}

void CFAttributedStringRemoveAttribute(CFMutableAttributedStringRef aStr, CFRange range, CFStringRef attrName) {
    ((void (*)(id, SEL, id, NSRange_))objc_msgSend)((id)aStr, sel("removeAttribute:range:"), (id)attrName, nsrange(range));
}

void CFAttributedStringReplaceAttributedString(CFMutableAttributedStringRef aStr, CFRange range, CFAttributedStringRef replacement) {
    ((void (*)(id, SEL, NSRange_, id))objc_msgSend)((id)aStr, sel("replaceCharactersInRange:withAttributedString:"), nsrange(range), (id)replacement);
}

void CFAttributedStringBeginEditing(CFMutableAttributedStringRef aStr) {
    send0((id)aStr, "beginEditing");
}

void CFAttributedStringEndEditing(CFMutableAttributedStringRef aStr) {
    send0((id)aStr, "endEditing");
}
