// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@class NSURL, NSError, NSFileHandle, NSString, NSAppleEventDescriptor;

NS_ASSUME_NONNULL_BEGIN

typedef void (^NSUserScriptTaskCompletionHandler)(NSError *_Nullable error);

/* A script the user put in the application's scripts folder
   (NSApplicationScriptsDirectory), run outside the application: made as
   the kind its file is (a Unix executable, an AppleScript, an Automator
   workflow). */
@interface NSUserScriptTask : NSObject
- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error;
@property (readonly, copy) NSURL *scriptURL;
- (void)executeWithCompletionHandler:(nullable NSUserScriptTaskCompletionHandler)handler;
@end

typedef void (^NSUserUnixTaskCompletionHandler)(NSError *_Nullable error);

@interface NSUserUnixTask : NSUserScriptTask
@property (nullable, retain) NSFileHandle *standardInput;
@property (nullable, retain) NSFileHandle *standardOutput;
@property (nullable, retain) NSFileHandle *standardError;
- (void)executeWithArguments:(nullable NSArray<NSString *> *)arguments completionHandler:(nullable NSUserUnixTaskCompletionHandler)handler;
@end

typedef void (^NSUserAppleScriptTaskCompletionHandler)(NSAppleEventDescriptor *_Nullable result, NSError *_Nullable error);

@interface NSUserAppleScriptTask : NSUserScriptTask
- (void)executeWithAppleEvent:(nullable NSAppleEventDescriptor *)event completionHandler:(nullable NSUserAppleScriptTaskCompletionHandler)handler;
@end

typedef void (^NSUserAutomatorTaskCompletionHandler)(id _Nullable result, NSError *_Nullable error);

@interface NSUserAutomatorTask : NSUserScriptTask
@property (nullable, copy) NSDictionary<NSString *, id> *variables;
- (void)executeWithInput:(nullable id<NSSecureCoding>)input completionHandler:(nullable NSUserAutomatorTaskCompletionHandler)handler;
@end

NS_ASSUME_NONNULL_END
