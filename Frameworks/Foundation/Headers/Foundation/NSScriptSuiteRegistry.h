// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSZone.h>
#import <MacTypes.h>

@class NSBundle, NSData, NSScriptClassDescription, NSScriptCommandDescription, NSString;

NS_ASSUME_NONNULL_BEGIN

/* The scriptable classes and commands, by suite: the standard ones, and
   an application's from its scripting definition (OSAScriptingDefinition,
   an sdef) or its .scriptSuite files. */
@interface NSScriptSuiteRegistry : NSObject

+ (NSScriptSuiteRegistry *)sharedScriptSuiteRegistry;
+ (void)setSharedScriptSuiteRegistry:(NSScriptSuiteRegistry *)registry;

- (void)loadSuitesFromBundle:(NSBundle *)bundle;
- (void)loadSuiteWithDictionary:(NSDictionary *)suiteDeclaration fromBundle:(NSBundle *)bundle;
- (void)registerClassDescription:(NSScriptClassDescription *)classDescription;
- (void)registerCommandDescription:(NSScriptCommandDescription *)commandDescription;

@property (readonly, copy) NSArray<NSString *> *suiteNames;
- (FourCharCode)appleEventCodeForSuite:(NSString *)suiteName;
- (nullable NSBundle *)bundleForSuite:(NSString *)suiteName;
- (nullable NSDictionary<NSString *, NSScriptClassDescription *> *)classDescriptionsInSuite:(NSString *)suiteName;
- (nullable NSDictionary<NSString *, NSScriptCommandDescription *> *)commandDescriptionsInSuite:(NSString *)suiteName;
- (nullable NSString *)suiteForAppleEventCode:(FourCharCode)appleEventCode;
- (nullable NSScriptClassDescription *)classDescriptionWithAppleEventCode:(FourCharCode)appleEventCode;
- (nullable NSScriptCommandDescription *)commandDescriptionWithAppleEventClass:(FourCharCode)appleEventClassCode andAppleEventCode:(FourCharCode)appleEventIDCode;
- (nullable NSData *)aeteResource:(NSString *)languageName;

@end

NS_ASSUME_NONNULL_END
