// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC. Types are kept once per identifier.

#import <UniformTypeIdentifiers/UTType.h>
#import <CoreServices/CoreServices.h>

@implementation UTType {
    NSString *_identifier;
}

static NSMutableDictionary<NSString *, UTType *> *sTypes;

+ (UTType *)typeWithIdentifier:(NSString *)identifier {
    if (identifier.length == 0)
        return nil;
    @synchronized (self) {
        if (sTypes == nil)
            sTypes = [NSMutableDictionary dictionary];
        UTType *type = sTypes[identifier.lowercaseString];
        if (type == nil) {
            type = [[UTType alloc] init];
            type->_identifier = [identifier copy];
            sTypes[identifier.lowercaseString] = type;
        }
        return type;
    }
}

+ (UTType *)typeWithTag:(CFStringRef)tagClass tag:(NSString *)tag conformingTo:(UTType *)supertype {
    if (tag == nil)
        return nil;
    NSString *identifier = CFBridgingRelease(UTTypeCreatePreferredIdentifierForTag(tagClass, (__bridge CFStringRef)tag,
        (__bridge CFStringRef)supertype.identifier));
    return [self typeWithIdentifier:identifier];
}

+ (UTType *)typeWithFilenameExtension:(NSString *)filenameExtension {
    return [self typeWithFilenameExtension:filenameExtension conformingToType:[UTType typeWithIdentifier:@"public.data"]];
}

+ (UTType *)typeWithFilenameExtension:(NSString *)filenameExtension conformingToType:(UTType *)supertype {
    return [self typeWithTag:kUTTagClassFilenameExtension tag:filenameExtension conformingTo:supertype];
}

+ (UTType *)typeWithMIMEType:(NSString *)mimeType {
    return [self typeWithMIMEType:mimeType conformingToType:[UTType typeWithIdentifier:@"public.data"]];
}

+ (UTType *)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype {
    return [self typeWithTag:kUTTagClassMIMEType tag:mimeType conformingTo:supertype];
}

+ (NSArray *)typesWithTag:(NSString *)tag tagClass:(NSString *)tagClass conformingToType:(UTType *)supertype {
    NSArray *identifiers = CFBridgingRelease(UTTypeCreateAllIdentifiersForTag((__bridge CFStringRef)tagClass, (__bridge CFStringRef)tag,
        (__bridge CFStringRef)supertype.identifier));
    NSMutableArray *types = [NSMutableArray array];
    for (NSString *identifier in identifiers)
        [types addObject:[self typeWithIdentifier:identifier]];
    return types;
}

+ (BOOL)supportsSecureCoding { return YES; }
- (id)copyWithZone:(NSZone *)zone { return self; }
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:_identifier forKey:@"UTTypeIdentifier"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    return [UTType typeWithIdentifier:[coder decodeObjectOfClass:[NSString class] forKey:@"UTTypeIdentifier"]];
}

- (NSString *)identifier { return _identifier; }
- (NSString *)preferredFilenameExtension {
    return CFBridgingRelease(UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)_identifier, kUTTagClassFilenameExtension));
}
- (NSString *)preferredMIMEType {
    return CFBridgingRelease(UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)_identifier, kUTTagClassMIMEType));
}
- (NSString *)localizedDescription {
    return CFBridgingRelease(UTTypeCopyDescription((__bridge CFStringRef)_identifier));
}
- (BOOL)isDeclared { return UTTypeIsDeclared((__bridge CFStringRef)_identifier); }
- (BOOL)isDynamic { return UTTypeIsDynamic((__bridge CFStringRef)_identifier); }
- (BOOL)isPublicType { return [_identifier hasPrefix:@"public."]; }

/* Every type it conforms to, its own conformances first. */
- (NSSet *)supertypes {
    NSMutableSet *result = [NSMutableSet set];
    NSMutableArray *queue = [NSMutableArray arrayWithObject:_identifier];
    while (queue.count > 0) {
        NSString *identifier = queue.firstObject;
        [queue removeObjectAtIndex:0];
        NSDictionary *declaration = CFBridgingRelease(UTTypeCopyDeclaration((__bridge CFStringRef)identifier));
        id parents = declaration[(__bridge NSString *)kUTTypeConformsToKey];
        if ([parents isKindOfClass:[NSString class]])
            parents = @[parents];
        for (NSString *parent in parents) {
            UTType *type = [UTType typeWithIdentifier:parent];
            if (type != nil && ![result containsObject:type]) {
                [result addObject:type];
                [queue addObject:parent];
            }
        }
    }
    return result;
}

- (BOOL)conformsToType:(UTType *)type {
    return type != nil && UTTypeConformsTo((__bridge CFStringRef)_identifier, (__bridge CFStringRef)type.identifier);
}
- (BOOL)isSupertypeOfType:(UTType *)type { return [type conformsToType:self] && ![type isEqual:self]; }
- (BOOL)isSubtypeOfType:(UTType *)type { return [self conformsToType:type] && ![type isEqual:self]; }
- (NSUInteger)hash { return _identifier.lowercaseString.hash; }
- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[UTType class]] && [_identifier caseInsensitiveCompare:[other identifier]] == NSOrderedSame;
}
- (NSString *)description { return _identifier; }
@end

@implementation NSString (UTAdditions)
- (NSString *)stringByAppendingPathComponent:(NSString *)partialName conformingToType:(UTType *)contentType {
    NSString *path = [self stringByAppendingPathComponent:partialName];
    return [[UTType typeWithFilenameExtension:path.pathExtension] conformsToType:contentType] ? path
           : [path stringByAppendingPathExtensionForType:contentType];
}
- (NSString *)stringByAppendingPathExtensionForType:(UTType *)contentType {
    NSString *extension = contentType.preferredFilenameExtension;
    return extension.length ? [self stringByAppendingPathExtension:extension] : self;
}
@end

@implementation NSURL (UTAdditions)
- (NSURL *)URLByAppendingPathComponent:(NSString *)partialName conformingToType:(UTType *)contentType {
    NSURL *url = [self URLByAppendingPathComponent:partialName];
    return [[UTType typeWithFilenameExtension:url.pathExtension] conformsToType:contentType] ? url
           : [url URLByAppendingPathExtensionForType:contentType];
}
- (NSURL *)URLByAppendingPathExtensionForType:(UTType *)contentType {
    NSString *extension = contentType.preferredFilenameExtension;
    return extension.length ? [self URLByAppendingPathExtension:extension] : self;
}
@end
