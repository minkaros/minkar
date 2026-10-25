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

/* Dictionary representations of CGPoint, CGSize and CGRect. */

#include <CoreGraphics/CGGeometry.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

static CFDictionaryRef create_dictionary(const CFStringRef *keys,
                                         const CGFloat *values, int count)
{
    CFMutableDictionaryRef dict;
    int i;

    dict = CFDictionaryCreateMutable(NULL, count, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; dict != NULL && i < count; i++) {
        double v = (double)values[i];
        CFNumberRef num = CFNumberCreate(NULL, kCFNumberDoubleType, &v);

        if (num == NULL) {
            CFRelease(dict);
            return NULL;
        }
        CFDictionarySetValue(dict, keys[i], num);
        CFRelease(num);
    }
    return dict;
}

static bool read_dictionary(CFDictionaryRef dict, const CFStringRef *keys,
                            CGFloat *values, int count)
{
    int i;

    if (dict == NULL)
        return false;
    for (i = 0; i < count; i++) {
        CFTypeRef num = CFDictionaryGetValue(dict, keys[i]);
        double v;

        if (num == NULL || CFGetTypeID(num) != CFNumberGetTypeID() ||
            !CFNumberGetValue((CFNumberRef)num, kCFNumberDoubleType, &v))
            return false;
        values[i] = (CGFloat)v;
    }
    return true;
}

#define KEY_X       CFSTR("X")
#define KEY_Y       CFSTR("Y")
#define KEY_WIDTH   CFSTR("Width")
#define KEY_HEIGHT  CFSTR("Height")

CFDictionaryRef CGPointCreateDictionaryRepresentation(CGPoint point)
{
    const CFStringRef keys[2] = { KEY_X, KEY_Y };
    const CGFloat values[2] = { point.x, point.y };

    return create_dictionary(keys, values, 2);
}

bool CGPointMakeWithDictionaryRepresentation(CFDictionaryRef dict, CGPoint *point)
{
    const CFStringRef keys[2] = { KEY_X, KEY_Y };
    CGFloat v[2];

    if (point == NULL || !read_dictionary(dict, keys, v, 2))
        return false;
    *point = CGPointMake(v[0], v[1]);
    return true;
}

CFDictionaryRef CGSizeCreateDictionaryRepresentation(CGSize size)
{
    const CFStringRef keys[2] = { KEY_WIDTH, KEY_HEIGHT };
    const CGFloat values[2] = { size.width, size.height };

    return create_dictionary(keys, values, 2);
}

bool CGSizeMakeWithDictionaryRepresentation(CFDictionaryRef dict, CGSize *size)
{
    const CFStringRef keys[2] = { KEY_WIDTH, KEY_HEIGHT };
    CGFloat v[2];

    if (size == NULL || !read_dictionary(dict, keys, v, 2))
        return false;
    *size = CGSizeMake(v[0], v[1]);
    return true;
}

CFDictionaryRef CGRectCreateDictionaryRepresentation(CGRect rect)
{
    const CFStringRef keys[4] = { KEY_X, KEY_Y, KEY_WIDTH, KEY_HEIGHT };
    const CGFloat values[4] = { rect.origin.x, rect.origin.y,
                                rect.size.width, rect.size.height };

    return create_dictionary(keys, values, 4);
}

bool CGRectMakeWithDictionaryRepresentation(CFDictionaryRef dict, CGRect *rect)
{
    const CFStringRef keys[4] = { KEY_X, KEY_Y, KEY_WIDTH, KEY_HEIGHT };
    CGFloat v[4];

    if (rect == NULL || !read_dictionary(dict, keys, v, 4))
        return false;
    *rect = CGRectMake(v[0], v[1], v[2], v[3]);
    return true;
}
