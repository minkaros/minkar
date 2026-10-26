/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInputStream.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSOutputStream.h>
#import <Foundation/NSString.h>
#import <Foundation/FoundationErrors.h>
#import <Foundation/NSRaise.h>
#import "NSPropertyListWriter_xml1.h"
#import "NSPropertyListWriter_vintage.h"
#import "NSPropertyListWriter_binary1.h"
#import "NSPropertyListReader_xml1.h"
#import "NSPropertyListReader_vintage.h"
#import "NSPropertyListReader_binary1.h"

// Minkar: the binary format can be written (it returned nil), the methods
// that take NSError exist, the mutability option is honoured, and
// propertyList:isValidForFormat: checks the types.

@implementation NSPropertyListSerialization

static BOOL validFor(id plist, NSPropertyListFormat format) {
    if ([plist isKindOfClass:[NSString class]] || [plist isKindOfClass:[NSData class]])
        return YES;
    if ([plist isKindOfClass:[NSArray class]]) {
        for (id item in plist)
            if (!validFor(item, format))
                return NO;
        return YES;
    }
    if ([plist isKindOfClass:[NSDictionary class]]) {
        for (id key in plist)
            if (![key isKindOfClass:[NSString class]] || !validFor([plist objectForKey:key], format))
                return NO;
        return YES;
    }
    // Numbers and dates: not in the old OpenStep format.
    if ([plist isKindOfClass:[NSNumber class]] || [plist isKindOfClass:[NSDate class]])
        return format != NSPropertyListOpenStepFormat;
    return NO;
}

+(BOOL)propertyList:plist isValidForFormat:(NSPropertyListFormat)format {
   return plist != nil && validFor(plist, format);
}

static NSError *plistError(NSInteger code, NSString *why) {
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code
        userInfo:[NSDictionary dictionaryWithObject:why forKey:NSDebugDescriptionErrorKey]];
}

+(NSData *)dataWithPropertyList:plist format:(NSPropertyListFormat)format
                        options:(NSPropertyListWriteOptions)opt error:(NSError **)error {
   NSString *why = nil;
   NSData   *data = nil;

   if (plist == nil || !validFor(plist, format)) {
    if (error != NULL)
     *error = plistError(NSPropertyListWriteInvalidError,
        [NSString stringWithFormat:@"Property list invalid for format: %lu", (unsigned long)format]);
    return nil;
   }
   switch(format){
    case NSPropertyListOpenStepFormat:
     data = [NSPropertyListWriter_vintage dataWithPropertyList:plist];
     break;
    case NSPropertyListXMLFormat_v1_0:
     data = [NSPropertyListWriter_xml1 dataWithPropertyList:plist];
     break;
    case NSPropertyListBinaryFormat_v1_0:
     data = [NSPropertyListWriter_binary1 dataWithPropertyList:plist error:&why];
     break;
    default:
     why = [NSString stringWithFormat:@"Unknown property list format %lu", (unsigned long)format];
     break;
   }
   if (data == nil && error != NULL)
    *error = plistError(NSPropertyListWriteInvalidError, why ? why : @"Property list could not be written");
   return data;
}

+(NSData *)dataFromPropertyList:plist format:(NSPropertyListFormat)format errorDescription:(NSString **)errorDescriptionp {
   NSError *error = nil;
   NSData  *data = [self dataWithPropertyList:plist format:format options:0 error:&error];

   if (data == nil && errorDescriptionp != NULL)
    *errorDescriptionp = [[error userInfo] objectForKey:NSDebugDescriptionErrorKey];
   return data;
}

+(NSInteger)writePropertyList:plist toStream:(NSOutputStream *)stream format:(NSPropertyListFormat)format
                      options:(NSPropertyListWriteOptions)opt error:(NSError **)error {
   NSData     *data = [self dataWithPropertyList:plist format:format options:opt error:error];
   const uint8_t *bytes = [data bytes];
   NSUInteger  length = [data length], written = 0;

   if (data == nil)
    return 0;
   while (written < length) {
    NSInteger n = [stream write:bytes + written maxLength:length - written];
    if (n <= 0) {
     if (error != NULL)
      *error = [stream streamError] ? [stream streamError]
        : plistError(NSPropertyListWriteStreamError, @"The stream could not be written");
     return 0;
    }
    written += (NSUInteger)n;
   }
   return (NSInteger)written;
}

// A deep copy with mutable containers (and leaves).
static id mutableCopyOf(id plist, NSPropertyListMutabilityOptions mutability) {
    if ([plist isKindOfClass:[NSArray class]]) {
        NSMutableArray *result = [NSMutableArray arrayWithCapacity:[plist count]];
        for (id item in plist)
            [result addObject:mutableCopyOf(item, mutability)];
        return result;
    }
    if ([plist isKindOfClass:[NSDictionary class]]) {
        NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:[plist count]];
        for (id key in plist)
            [result setObject:mutableCopyOf([plist objectForKey:key], mutability) forKey:key];
        return result;
    }
    if (mutability == NSPropertyListMutableContainersAndLeaves &&
        ([plist isKindOfClass:[NSString class]] || [plist isKindOfClass:[NSData class]]))
        return [[plist mutableCopy] autorelease];
    return plist;
}

// The property list and its format, or nil.
static id readAny(NSData *data, NSPropertyListFormat *format) {
   id result = nil;

   @try {
    if ((result=[NSPropertyListReader_binary1 propertyListFromData:data])!=nil){
     if(format)*format=NSPropertyListBinaryFormat_v1_0;
     return result;
    }
    if ((result=[NSPropertyListReader_xml1 propertyListFromData:data])!=nil){
     if(format)*format=NSPropertyListXMLFormat_v1_0;
     return result;
    }
    if ((result=[NSPropertyListReader_vintage propertyListFromData:data])!=nil){
     if(format)*format=NSPropertyListOpenStepFormat;
     return result;
    }
   } @catch (NSException *e) {
    return nil;
   }
   return nil;
}

+(id)propertyListWithData:(NSData *)data options:(NSPropertyListReadOptions)opt
                   format:(NSPropertyListFormat *)format error:(NSError **)error {
   id result = (data != nil) ? readAny(data, format) : nil;

   if (result == nil) {
    if (error != NULL)
     *error = plistError(NSPropertyListReadCorruptError, @"The data is not a property list in a known format");
    return nil;
   }
   if (opt == NSPropertyListMutableContainers || opt == NSPropertyListMutableContainersAndLeaves)
    result = mutableCopyOf(result, opt);
   return result;
}

+propertyListFromData:(NSData *)data mutabilityOption:(NSPropertyListMutabilityOptions)mutability format:(NSPropertyListFormat *)format errorDescription:(NSString **)errorDescriptionp {
   NSError *error = nil;
   id       result = [self propertyListWithData:data options:mutability format:format error:&error];

   if (result == nil && errorDescriptionp != NULL)
    *errorDescriptionp = [[error userInfo] objectForKey:NSDebugDescriptionErrorKey];
   return result;
}

+(id)propertyListWithStream:(NSInputStream *)stream options:(NSPropertyListReadOptions)opt
                     format:(NSPropertyListFormat *)format error:(NSError **)error {
   NSMutableData *data = [NSMutableData data];
   uint8_t        chunk[4096];

   for (;;) {
    NSInteger n = [stream read:chunk maxLength:sizeof(chunk)];
    if (n == 0)
     break;
    if (n < 0) {
     if (error != NULL)
      *error = [stream streamError] ? [stream streamError]
        : plistError(NSPropertyListReadStreamError, @"The stream could not be read");
     return nil;
    }
    [data appendBytes:chunk length:(NSUInteger)n];
   }
   return [self propertyListWithData:data options:opt format:format error:error];
}

@end
