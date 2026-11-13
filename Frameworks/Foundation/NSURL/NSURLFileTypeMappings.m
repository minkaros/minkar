/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSURLFileTypeMappings: MIME types and file extensions, each for the
 * other. Not public API, but apps use it (Firefox's helper apps); the
 * answers are the system's type database (CoreServices' UTType tags).
 */

#import <Foundation/NSArray.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#include <CoreServices/CoreServices.h>

@interface NSURLFileTypeMappings : NSObject
+ (NSURLFileTypeMappings *)sharedMappings;
- (NSString *)MIMETypeForExtension:(NSString *)extension;
- (NSString *)preferredExtensionForMIMEType:(NSString *)MIMEType;
- (NSArray<NSString *> *)extensionsForMIMEType:(NSString *)MIMEType;
@end

@implementation NSURLFileTypeMappings

+ (NSURLFileTypeMappings *)sharedMappings {
    static NSURLFileTypeMappings *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        shared = [[NSURLFileTypeMappings alloc] init];
    });
    return shared;
}

- (NSString *)MIMETypeForExtension:(NSString *)extension {
    if ([extension length] == 0)
        return nil;
    CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
        (__bridge CFStringRef)[extension lowercaseString], NULL);
    if (type == NULL)
        return nil;
    NSString *mime = (__bridge_transfer NSString *)UTTypeCopyPreferredTagWithClass(type, kUTTagClassMIMEType);
    CFRelease(type);
    return mime;
}

- (NSArray<NSString *> *)extensionsForMIMEType:(NSString *)MIMEType {
    if ([MIMEType length] == 0)
        return nil;
    CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
        (__bridge CFStringRef)[MIMEType lowercaseString], NULL);
    if (type == NULL)
        return nil;
    NSArray *extensions = (__bridge_transfer NSArray *)UTTypeCopyAllTagsWithClass(type, kUTTagClassFilenameExtension);
    CFRelease(type);
    return [extensions count] > 0 ? extensions : nil;
}

- (NSString *)preferredExtensionForMIMEType:(NSString *)MIMEType {
    return [[self extensionsForMIMEType:MIMEType] firstObject];
}

@end
