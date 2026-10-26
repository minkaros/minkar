/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

@class NSData, NSError, NSString, NSInputStream, NSOutputStream;

// The values are those of macOS (and CoreFoundation's kCFPropertyList...).
typedef NS_OPTIONS(NSUInteger, NSPropertyListMutabilityOptions) {
    NSPropertyListImmutable = 0,
    NSPropertyListMutableContainers = 1,
    NSPropertyListMutableContainersAndLeaves = 2,
};

typedef NS_ENUM(NSUInteger, NSPropertyListFormat) {
    NSPropertyListOpenStepFormat = 1,
    NSPropertyListXMLFormat_v1_0 = 100,
    NSPropertyListBinaryFormat_v1_0 = 200,
};

typedef NSPropertyListMutabilityOptions NSPropertyListReadOptions;
typedef NSUInteger NSPropertyListWriteOptions;

@interface NSPropertyListSerialization : NSObject

+ (BOOL)propertyList:(id)plist isValidForFormat:(NSPropertyListFormat)format;

+ (NSData *)dataWithPropertyList:(id)plist format:(NSPropertyListFormat)format
                         options:(NSPropertyListWriteOptions)opt error:(NSError **)error;
+ (NSInteger)writePropertyList:(id)plist toStream:(NSOutputStream *)stream
                        format:(NSPropertyListFormat)format options:(NSPropertyListWriteOptions)opt
                         error:(NSError **)error;
+ (id)propertyListWithData:(NSData *)data options:(NSPropertyListReadOptions)opt
                    format:(NSPropertyListFormat *)format error:(NSError **)error;
+ (id)propertyListWithStream:(NSInputStream *)stream options:(NSPropertyListReadOptions)opt
                      format:(NSPropertyListFormat *)format error:(NSError **)error;

// Deprecated on macOS since 10.6.
+ (NSData *)dataFromPropertyList:(id)plist format:(NSPropertyListFormat)format
                errorDescription:(NSString **)errorDescriptionp;
+ (id)propertyListFromData:(NSData *)data mutabilityOption:(NSPropertyListMutabilityOptions)mutability
                    format:(NSPropertyListFormat *)format errorDescription:(NSString **)errorDescriptionp;

@end
