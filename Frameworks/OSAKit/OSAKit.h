// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Scripts in an OSA language. There is no scripting component yet:
// compiling and running fail and say so (as NSAppleScript does).

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const OSAScriptErrorMessageKey;
FOUNDATION_EXPORT NSString *const OSAScriptErrorBriefMessageKey;
FOUNDATION_EXPORT NSString *const OSAScriptErrorNumberKey;
FOUNDATION_EXPORT NSString *const OSAScriptErrorAppNameKey;
FOUNDATION_EXPORT NSString *const OSAScriptErrorRangeKey;
FOUNDATION_EXPORT NSString *const OSAScriptErrorMessage;
FOUNDATION_EXPORT NSString *const OSAScriptErrorNumber;
FOUNDATION_EXPORT NSString *const OSAScriptErrorAppName;
FOUNDATION_EXPORT NSString *const OSAScriptErrorBriefMessage;
FOUNDATION_EXPORT NSString *const OSAScriptErrorRange;

@interface OSALanguage : NSObject
+ (NSArray<OSALanguage *> *)availableLanguages;
+ (nullable OSALanguage *)languageForName:(NSString *)name;
+ (nullable OSALanguage *)defaultLanguage;
@property (nullable, readonly, copy) NSString *name;
@end

@interface OSAScript : NSObject <NSCopying>
- (instancetype)initWithSource:(NSString *)source;
- (instancetype)initWithSource:(NSString *)source language:(nullable OSALanguage *)language;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url error:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
@property (readonly, copy) NSString *source;
@property (nullable, readonly, copy) NSURL *url;
@property (nullable, readonly, retain) OSALanguage *language;
@property (readonly, getter=isCompiled) BOOL compiled;
- (BOOL)compileAndReturnError:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (nullable NSAppleEventDescriptor *)executeAndReturnError:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (nullable NSAppleEventDescriptor *)executeAppleEvent:(NSAppleEventDescriptor *)event
                                                 error:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (nullable NSAppleEventDescriptor *)executeHandlerWithName:(NSString *)name arguments:(NSArray *)arguments
                                                      error:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
@end

NS_ASSUME_NONNULL_END
