// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSClassDescription.h>
#import <MacTypes.h>

@class NSScriptCommandDescription;

NS_ASSUME_NONNULL_BEGIN

/* A scriptable class: its Apple event code, its attributes and
   relationships (each with a type and a code), the commands it supports,
   from a suite (NSScriptSuiteRegistry). */
@interface NSScriptClassDescription : NSClassDescription

+ (nullable NSScriptClassDescription *)classDescriptionForClass:(Class)aClass;

- (nullable instancetype)initWithSuiteName:(NSString *)suiteName className:(NSString *)className dictionary:(nullable NSDictionary *)classDeclaration NS_DESIGNATED_INITIALIZER;

@property (nullable, readonly, copy) NSString *suiteName;
@property (nullable, readonly, copy) NSString *className;
@property (nullable, readonly, copy) NSString *implementationClassName;
@property (nullable, readonly, retain) NSScriptClassDescription *superclassDescription;
@property (readonly) FourCharCode appleEventCode;
@property (nullable, readonly, copy) NSString *defaultSubcontainerAttributeKey;

- (BOOL)matchesAppleEventCode:(FourCharCode)appleEventCode;
- (BOOL)supportsCommand:(NSScriptCommandDescription *)commandDescription;
- (nullable SEL)selectorForCommand:(NSScriptCommandDescription *)commandDescription;
- (nullable NSString *)typeForKey:(NSString *)key;
- (nullable NSScriptClassDescription *)classDescriptionForKey:(NSString *)key;
- (FourCharCode)appleEventCodeForKey:(NSString *)key;
- (nullable NSString *)keyWithAppleEventCode:(FourCharCode)appleEventCode;
- (BOOL)isLocationRequiredToCreateForKey:(NSString *)toManyRelationshipKey;
- (BOOL)hasPropertyForKey:(NSString *)key;
- (BOOL)hasOrderedToManyRelationshipForKey:(NSString *)key;
- (BOOL)hasReadablePropertyForKey:(NSString *)key;
- (BOOL)hasWritablePropertyForKey:(NSString *)key;
- (BOOL)isReadOnlyKey:(NSString *)key;

@end

@interface NSObject (NSScriptClassDescription)
/* The code of its class's scripting description (0 without one). */
@property (readonly) FourCharCode classCode;
/* Its class's scripting name, or the class's name. */
@property (readonly, copy) NSString *className;
@end

NS_ASSUME_NONNULL_END
