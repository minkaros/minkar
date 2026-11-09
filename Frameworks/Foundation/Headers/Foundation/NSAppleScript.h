// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSDictionary.h>

@class NSAppleEventDescriptor, NSDictionary, NSString, NSURL;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSAppleScriptErrorMessage;
FOUNDATION_EXPORT NSString *const NSAppleScriptErrorNumber;
FOUNDATION_EXPORT NSString *const NSAppleScriptErrorAppName;
FOUNDATION_EXPORT NSString *const NSAppleScriptErrorBriefMessage;
FOUNDATION_EXPORT NSString *const NSAppleScriptErrorRange;

/* Scripts in the AppleScript language. There is no AppleScript component:
   compiling and running fail with an error dictionary that says so. */
@interface NSAppleScript : NSObject <NSCopying> {
    NSString *_source;
    NSURL *_url;
}

- (nullable instancetype)initWithContentsOfURL:(NSURL *)url error:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (nullable instancetype)initWithSource:(NSString *)source NS_DESIGNATED_INITIALIZER;

@property (nullable, readonly, copy) NSString *source;
@property (readonly, getter=isCompiled) BOOL compiled;

- (BOOL)compileAndReturnError:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (NSAppleEventDescriptor *)executeAndReturnError:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;
- (NSAppleEventDescriptor *)executeAppleEvent:(NSAppleEventDescriptor *)event error:(NSDictionary<NSString *, id> *_Nullable *_Nullable)errorInfo;

@end

NS_ASSUME_NONNULL_END
