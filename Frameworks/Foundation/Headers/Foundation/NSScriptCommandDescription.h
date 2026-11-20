// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSZone.h>
#import <MacTypes.h>

@class NSScriptCommand, NSString;

NS_ASSUME_NONNULL_BEGIN

/* A scriptable command: its Apple event codes, the class that carries it
   out, its arguments and result, from a suite. */
@interface NSScriptCommandDescription : NSObject <NSCoding>

- (instancetype)init NS_UNAVAILABLE;
- (nullable instancetype)initWithSuiteName:(NSString *)suiteName commandName:(NSString *)commandName dictionary:(nullable NSDictionary *)commandDeclaration NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property (readonly, copy) NSString *suiteName;
@property (readonly, copy) NSString *commandName;
@property (readonly) FourCharCode appleEventClassCode;
@property (readonly) FourCharCode appleEventCode;
@property (readonly, copy) NSString *commandClassName;
@property (nullable, readonly, copy) NSString *returnType;
@property (readonly) FourCharCode appleEventCodeForReturnType;
@property (readonly, copy) NSArray<NSString *> *argumentNames;

- (nullable NSString *)typeForArgumentWithName:(NSString *)argumentName;
- (FourCharCode)appleEventCodeForArgumentWithName:(NSString *)argumentName;
- (BOOL)isOptionalArgumentWithName:(NSString *)argumentName;
- (NSScriptCommand *)createCommandInstance;
- (NSScriptCommand *)createCommandInstanceWithZone:(nullable NSZone *)zone;

@end

NS_ASSUME_NONNULL_END
