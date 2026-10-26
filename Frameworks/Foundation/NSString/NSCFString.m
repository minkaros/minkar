/*
 * Copyright (c) 2026 Zoe Knox.
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NSCFString.h"
#include <stdio.h>
#include <sys/param.h>
#include <objc/runtime.h>

#import <Foundation/NSException.h>
#import <Foundation/NSStringHashing.h>
#import <Foundation/NSRaiseException.h>

#define TRACE(...) do { printf("%s: ", object_getClassName(self)); printf(__VA_ARGS__); } while(0);

extern unichar *NSUTF8ToUnicode(const char *,NSUInteger,NSUInteger *,NSZone *);
extern char *NSUnicodeToUTF8(const unichar *,NSUInteger,BOOL,NSUInteger *,NSZone *,BOOL);

@implementation NSCFString

// NSCFString objects are CoreFoundation strings (CFBridging.c registers
// this class for CFString), so they are made and read by CoreFoundation,
// which knows their layout. CF never calls back into Objective-C here
// (CF_IS_OBJC is false), so there is no recursion.
// Minkar: these replace code that built and read CFString's layout by
// hand; it counted UTF-8 bytes as characters, read a length byte as a
// character and cut lengths to 8 bits.

extern CFIndex _CFStringGetLength2(CFStringRef str);
extern int _CFStringCheckAndGetCharacterAtIndex(CFStringRef str, CFIndex idx, UniChar *ch);
extern int _CFStringCheckAndGetCharacters(CFStringRef str, CFRange range, UniChar *buffer);

NSString *NSCFStringNewWithCharacters(NSZone *zone, const unichar *characters,
        NSUInteger length, BOOL freeWhenDone)
{
    (void)zone;
    CFStringRef string = CFStringCreateWithCharacters(NULL, (const UniChar *)characters,
                                                      (CFIndex)length);
    if (freeWhenDone && characters != NULL)
        NSZoneFree(NULL, (void *)characters);
    return (NSString *)string;
}

NSString *NSCFStringNewWithBytes(NSZone *zone, const char *bytes,
         NSUInteger length, NSStringEncoding encoding)
{
    (void)zone;
    CFStringEncoding cfEncoding = CFStringConvertNSStringEncodingToEncoding(encoding);
    CFStringRef string = NULL;
    if (cfEncoding != kCFStringEncodingInvalidId)
        string = CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, (CFIndex)length,
                                         cfEncoding, false);
    if (string == NULL)     // never fail: every byte is a Latin 1 character
        string = CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, (CFIndex)length,
                                         kCFStringEncodingISOLatin1, false);
    return (NSString *)string;
}

NSUInteger NSGetCFStringWithMaxLength(const unichar *characters, 
        NSUInteger length, NSUInteger *location, char *cString, 
        NSUInteger maxLength, BOOL lossy)
{
    NSUInteger i, result = 0;

    if (length + 1 > maxLength) {
        cString[0] = '\0';
        return NSNotFound;
    }

    for (i = 0; i < length && result <= maxLength; i++) {
        const unichar code = characters[i];

        if (code < 256)
            cString[result++] = code;
        else {
            if(lossy)
                cString[result++] = '\0';
            else {
                return NSNotFound;
            }
        }
    }
    cString[result]='\0';

    if (location)
        *location = i;
    return result;
}

// Memory of CF's strings is CoreFoundation's: retain and release are
// CFRetain and CFRelease, and CF frees the string when its count reaches
// zero. Minkar: these were empty, so no string made from Objective-C was
// ever freed, and copies were not retained.
//
// Only instances of NSCFString itself (and constant strings) are CF
// strings. The older string classes that inherit from it (NSString_unicode,
// NSString_isoLatin1, NSDarwinString...) are plain Objective-C objects:
// they keep NSObject's memory management. CFRelease would send them
// -release again, without end.
extern int __CFConstantStringClassReference[];

static inline BOOL isCFString(id self)
{
    Class cls = object_getClass(self);
    return cls == [NSCFString self] ||
           (void *)cls == (void *)__CFConstantStringClassReference;
}

-copy {
    return [self copyWithZone:NULL];
}

-copyWithZone:(NSZone *)zone {
    if (isCFString(self))
        return (id)CFStringCreateCopy(NULL, (CFStringRef)self);
    // An immutable string: a copy is the string itself.
    return [self retain];
}

-retain {
    if (!isCFString(self))
        return [super retain];
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release
{
    if (!isCFString(self)) {
        [super release];
        return;
    }
    CFRelease((CFTypeRef)self);
}

-(NSUInteger)retainCount {
    if (!isCFString(self))
        return [super retainCount];
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

// Never sent to a CF string: CoreFoundation frees it itself.
-(void)dealloc {
    if (isCFString(self))
        return;
    [super dealloc];
}

- initWithCharactersNoCopy:(unichar *)characters length:(NSUInteger)length freeWhenDone:(BOOL)freeWhenDone
{
    NSDeallocateObject(self);
    return NSCFStringNewWithCharacters(NULL, characters, length, freeWhenDone);
}

-(NSUInteger)length {
    return (NSUInteger)_CFStringGetLength2((CFStringRef)self);
}

-(unichar)characterAtIndex:(NSUInteger)location {
    UniChar c = 0;
    if (_CFStringCheckAndGetCharacterAtIndex((CFStringRef)self, (CFIndex)location, &c) != 0) {
        NSRaiseException(NSRangeException, self, _cmd,
            @"index %lu beyond length %lu", (unsigned long)location, (unsigned long)[self length]);
    }
    return c;
}

-(NSStringEncoding)fastestEncoding {
    return CFStringConvertEncodingToNSStringEncoding(
        CFStringGetFastestEncoding((CFStringRef)self));
}

// The 8-bit contents, when CF keeps them so; NULL otherwise.
-(const char *)_fastCStringContents:(BOOL)getContents {
    (void)getContents;
    return CFStringGetCStringPtr((CFStringRef)self, kCFStringEncodingASCII);
}

// The UTF-16 contents, when CF keeps them so; NULL otherwise.
-(unichar *)_fastCharacterContents {
    return (unichar *)CFStringGetCharactersPtr((CFStringRef)self);
}

-(BOOL)_encodingCantBeStoredInEightBitCFString {
    return CFStringGetCharactersPtr((CFStringRef)self) != NULL;
}


-(void)getCharacters:(unichar *)buffer {
    return [self getCharacters:buffer range:NSMakeRange(0, [self length])];
}

-(void)getCharacters:(unichar *)buffer range:(NSRange)range {
    if (buffer == NULL)
        return;
    CFRange r = CFRangeMake((CFIndex)range.location, (CFIndex)range.length);
    if (_CFStringCheckAndGetCharacters((CFStringRef)self, r, (UniChar *)buffer) != 0) {
        NSRaiseException(NSRangeException, self, _cmd,
            @"range %@ beyond length %lu", NSStringFromRange(range), (unsigned long)[self length]);
    }
}

-(NSUInteger)hash {
    return CFStringHashNSString(self);
}

@end
