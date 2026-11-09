// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Uniform types as objects (API 20), on LaunchServices' type
// declarations.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface UTType : NSObject <NSCopying, NSSecureCoding>
+ (nullable UTType *)typeWithIdentifier:(NSString *)identifier;
+ (nullable UTType *)typeWithFilenameExtension:(NSString *)filenameExtension;
+ (nullable UTType *)typeWithFilenameExtension:(NSString *)filenameExtension conformingToType:(UTType *)supertype;
+ (nullable UTType *)typeWithMIMEType:(NSString *)mimeType;
+ (nullable UTType *)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype;
+ (NSArray<UTType *> *)typesWithTag:(NSString *)tag tagClass:(NSString *)tagClass conformingToType:(nullable UTType *)supertype;
@property (readonly, copy) NSString *identifier;
@property (nullable, readonly, copy) NSString *preferredFilenameExtension;
@property (nullable, readonly, copy) NSString *preferredMIMEType;
@property (nullable, readonly, copy) NSString *localizedDescription;
@property (readonly, copy) NSSet<UTType *> *supertypes;
@property (readonly, getter=isDeclared) BOOL declared;
@property (readonly, getter=isDynamic) BOOL dynamic;
@property (readonly, getter=isPublicType) BOOL publicType;
- (BOOL)conformsToType:(UTType *)type;
- (BOOL)isSupertypeOfType:(UTType *)type;
- (BOOL)isSubtypeOfType:(UTType *)type;
@end

@interface NSString (UTAdditions)
- (NSString *)stringByAppendingPathComponent:(NSString *)partialName conformingToType:(UTType *)contentType;
- (NSString *)stringByAppendingPathExtensionForType:(UTType *)contentType;
@end

@interface NSURL (UTAdditions)
- (NSURL *)URLByAppendingPathComponent:(NSString *)partialName conformingToType:(UTType *)contentType;
- (NSURL *)URLByAppendingPathExtensionForType:(UTType *)contentType;
@end

NS_ASSUME_NONNULL_END
