/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
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
 * The messages CoreFoundation's string functions send to Foundation's own
 * strings (NSMutableString_unicodePtr, NSString_unicode...), which are not
 * CF strings: CFString.c dispatches to them (CF_OBJC_FUNCDISPATCHV) as it
 * does for arrays and dictionaries.
 *
 * A mutation CF would do in place is done on a CF copy of the string, made
 * from its characters (a CF function given the string itself would send
 * the message again), and the result is set back.
 */

#import "NSStringCFBridging.h"
#import <Foundation/NSMutableString.h>
#import <CoreFoundation/CFString.h>
#import <CoreFoundation/CFStringEncodingExt.h>
#include <stdlib.h>

// A CF string with the characters of string. Returned retained.
static CFMutableStringRef copyAsCFString(NSString *string) {
    NSUInteger length = [string length];
    CFMutableStringRef copy = CFStringCreateMutable(NULL, 0);
    if (length == 0)
        return copy;
    unichar *characters = malloc(sizeof(unichar) * length);
    [string getCharacters:characters range:NSMakeRange(0, length)];
    CFStringAppendCharacters(copy, characters, (CFIndex)length);
    free(characters);
    return copy;
}

@implementation NSString (CFBridging)

// No contents CF could read in place.
- (const char *)_fastCStringContents:(BOOL)nullTerminated {
    return NULL;
}

- (unichar *)_fastCharacterContents {
    return NULL;
}

- (BOOL)_encodingCantBeStoredInEightBitCFString {
    NSUInteger length = [self length];
    for (NSUInteger i = 0; i < length; i++)
        if ([self characterAtIndex:i] > 0x7f)
            return YES;
    return NO;
}

// maxLength excludes the terminating NUL, which is written. CF converts:
// Foundation's own converters lack UTF-8 here.
- (BOOL)_getCString:(char *)buffer maxLength:(NSUInteger)maxLength encoding:(CFStringEncoding)encoding {
    CFMutableStringRef copy = copyAsCFString(self);
    Boolean ok = CFStringGetCString(copy, buffer, (CFIndex)maxLength + 1, encoding);
    CFRelease(copy);
    return ok;
}

- (CFStringEncoding)_smallestEncodingInCFStringEncoding {
    return CFStringConvertNSStringEncodingToEncoding([self smallestEncoding]);
}

- (CFStringEncoding)_fastestEncodingInCFStringEncoding {
    return CFStringConvertNSStringEncodingToEncoding([self fastestEncoding]);
}

@end

@implementation NSMutableString (CFBridging)

- (void)appendCharacters:(const unichar *)characters length:(NSUInteger)length {
    [self appendString:[NSString stringWithCharacters:characters length:length]];
}

// Eight-bit characters, as CF's default eight-bit encoding has them.
- (void)_cfAppendCString:(const unsigned char *)bytes length:(NSInteger)length {
    NSString *string = [[NSString alloc] initWithBytes:bytes length:(NSUInteger)length
                                              encoding:NSISOLatin1StringEncoding];
    if (string != nil)
        [self appendString:string];
    [string release];
}

#define IN_CF(operation) do { \
    CFMutableStringRef copy = copyAsCFString(self); \
    operation; \
    [self setString:(NSString *)copy]; \
    CFRelease(copy); \
} while (0)

- (void)_cfPad:(CFStringRef)padString length:(uint32_t)length padIndex:(uint32_t)indexIntoPad {
    IN_CF(CFStringPad(copy, padString, (CFIndex)length, (CFIndex)indexIntoPad));
}

- (void)_cfTrim:(CFStringRef)trimString {
    IN_CF(CFStringTrim(copy, trimString));
}

- (void)_cfTrimWS {
    IN_CF(CFStringTrimWhitespace(copy));
}

- (void)_cfLowercase:(const void *)locale {
    IN_CF(CFStringLowercase(copy, (CFLocaleRef)locale));
}

- (void)_cfUppercase:(const void *)locale {
    IN_CF(CFStringUppercase(copy, (CFLocaleRef)locale));
}

- (void)_cfCapitalize:(const void *)locale {
    IN_CF(CFStringCapitalize(copy, (CFLocaleRef)locale));
}

- (void)_cfNormalize:(CFStringNormalizationForm)form {
    IN_CF(CFStringNormalize(copy, form));
}

#undef IN_CF

@end
