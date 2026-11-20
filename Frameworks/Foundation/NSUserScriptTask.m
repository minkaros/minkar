// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSUserScriptTask.h>
#import <Foundation/Foundation.h>
#include <sys/stat.h>
#include <unistd.h>

/* Runs a handler on a queue of its own, as the reference does, so a
   caller waiting on the main thread is not blocked. */
static void callLater(void (^block)(void)) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), block);
}

static NSError *scriptError(NSInteger code, NSURL *url, NSString *reason) {
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    if (url != nil)
        [info setObject:url forKey:NSURLErrorKey];
    if (reason != nil)
        [info setObject:reason forKey:NSLocalizedFailureReasonErrorKey];
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:info];
}

@implementation NSUserScriptTask {
@protected
    NSURL *_scriptURL;
}

/* The kind of task its file is: an AppleScript by its extension, an
   Automator workflow by its, otherwise an executable file. A file that is
   not there, or not executable, is an error, as on the reference. */
- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error {
    NSString *path = [url path];
    NSString *extension = [[path pathExtension] lowercaseString];
    Class kind = [self class];
    struct stat st;

    if (path == nil || stat([path fileSystemRepresentation], &st) != 0) {
        if (error != NULL)
            *error = scriptError(NSFileReadNoSuchFileError, url, @"The script does not exist.");
        [self release];
        return nil;
    }
    if (kind == [NSUserScriptTask class]) {
        if ([extension isEqualToString:@"scpt"] || [extension isEqualToString:@"scptd"] || [extension isEqualToString:@"applescript"])
            kind = [NSUserAppleScriptTask class];
        else if ([extension isEqualToString:@"workflow"])
            kind = [NSUserAutomatorTask class];
        else
            kind = [NSUserUnixTask class];
    }
    if (kind == [NSUserUnixTask class] && (S_ISDIR(st.st_mode) || access([path fileSystemRepresentation], X_OK) != 0)) {
        if (error != NULL)
            *error = scriptError(NSFileReadCorruptFileError, url, @"The script is not executable.");
        [self release];
        return nil;
    }
    if (kind != [self class]) {
        [self release];
        self = [kind alloc];
    }
    if ((self = [super init]) != nil)
        _scriptURL = [url copy];
    return self;
}

- (void)dealloc {
    [_scriptURL release];
    [super dealloc];
}

- (NSURL *)scriptURL {
    return _scriptURL;
}

- (void)executeWithCompletionHandler:(NSUserScriptTaskCompletionHandler)handler {
    [self doesNotRecognizeSelector:_cmd];
}

@end

@implementation NSUserUnixTask {
    NSFileHandle *_standardInput, *_standardOutput, *_standardError;
}

- (void)dealloc {
    [_standardInput release];
    [_standardOutput release];
    [_standardError release];
    [super dealloc];
}

- (NSFileHandle *)standardInput { return _standardInput; }
- (void)setStandardInput:(NSFileHandle *)handle { [handle retain]; [_standardInput release]; _standardInput = handle; }
- (NSFileHandle *)standardOutput { return _standardOutput; }
- (void)setStandardOutput:(NSFileHandle *)handle { [handle retain]; [_standardOutput release]; _standardOutput = handle; }
- (NSFileHandle *)standardError { return _standardError; }
- (void)setStandardError:(NSFileHandle *)handle { [handle retain]; [_standardError release]; _standardError = handle; }

/* Run with its arguments and the handles given (none: the null device),
   the handler told when it ends: an error when it could not start or
   exited with a failure. */
- (void)executeWithArguments:(NSArray<NSString *> *)arguments completionHandler:(NSUserUnixTaskCompletionHandler)handler {
    NSTask *task = [[[NSTask alloc] init] autorelease];
    NSURL *url = _scriptURL;
    NSUserUnixTaskCompletionHandler done = [[handler copy] autorelease];

    [task setLaunchPath:[url path]];
    [task setArguments:(arguments != nil) ? arguments : [NSArray array]];
    [task setStandardInput:(_standardInput != nil) ? (id)_standardInput : (id)[NSFileHandle fileHandleWithNullDevice]];
    [task setStandardOutput:(_standardOutput != nil) ? (id)_standardOutput : (id)[NSFileHandle fileHandleWithNullDevice]];
    [task setStandardError:(_standardError != nil) ? (id)_standardError : (id)[NSFileHandle fileHandleWithNullDevice]];
    @try {
        [task launch];
    } @catch (NSException *exception) {
        if (done != nil) {
            NSError *error = scriptError(NSExecutableLoadError, url, [exception reason]);
            callLater(^{ done(error); });
        }
        return;
    }
    [task retain];
    callLater(^{
        [task waitUntilExit];
        NSError *error = nil;
        if ([task terminationReason] != NSTaskTerminationReasonExit || [task terminationStatus] != 0)
            error = scriptError(NSExecutableRuntimeMismatchError, url, [NSString stringWithFormat:@"The script ended with status %d.", [task terminationStatus]]);
        [task release];
        if (done != nil)
            done(error);
    });
}

- (void)executeWithCompletionHandler:(NSUserScriptTaskCompletionHandler)handler {
    [self executeWithArguments:nil completionHandler:handler];
}

@end

/* Minkar has no AppleScript or Automator engine yet: such a task is made
   (its file is there), and running it tells the handler it cannot run. */
@implementation NSUserAppleScriptTask

- (void)executeWithAppleEvent:(NSAppleEventDescriptor *)event completionHandler:(NSUserAppleScriptTaskCompletionHandler)handler {
    NSUserAppleScriptTaskCompletionHandler done = [[handler copy] autorelease];
    NSError *error = scriptError(NSFeatureUnsupportedError, _scriptURL, @"AppleScript scripts cannot be run on this system.");

    if (done != nil)
        callLater(^{ done(nil, error); });
}

- (void)executeWithCompletionHandler:(NSUserScriptTaskCompletionHandler)handler {
    NSUserScriptTaskCompletionHandler done = [[handler copy] autorelease];

    [self executeWithAppleEvent:nil completionHandler:^(NSAppleEventDescriptor *result, NSError *error) {
        if (done != nil)
            done(error);
    }];
}

@end

@implementation NSUserAutomatorTask {
    NSDictionary *_variables;
}

- (void)dealloc {
    [_variables release];
    [super dealloc];
}

- (NSDictionary *)variables { return _variables; }
- (void)setVariables:(NSDictionary *)variables { variables = [variables copy]; [_variables release]; _variables = variables; }

- (void)executeWithInput:(id<NSSecureCoding>)input completionHandler:(NSUserAutomatorTaskCompletionHandler)handler {
    NSUserAutomatorTaskCompletionHandler done = [[handler copy] autorelease];
    NSError *error = scriptError(NSFeatureUnsupportedError, _scriptURL, @"Automator workflows cannot be run on this system.");

    if (done != nil)
        callLater(^{ done(nil, error); });
}

- (void)executeWithCompletionHandler:(NSUserScriptTaskCompletionHandler)handler {
    NSUserScriptTaskCompletionHandler done = [[handler copy] autorelease];

    [self executeWithInput:nil completionHandler:^(id result, NSError *error) {
        if (done != nil)
            done(error);
    }];
}

@end
