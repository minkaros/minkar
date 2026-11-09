// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import <OSAKit/OSAKit.h>

NSString *const OSAScriptErrorMessageKey = @"OSAScriptErrorMessageKey";
NSString *const OSAScriptErrorBriefMessageKey = @"OSAScriptErrorBriefMessageKey";
NSString *const OSAScriptErrorNumberKey = @"OSAScriptErrorNumberKey";
NSString *const OSAScriptErrorAppNameKey = @"OSAScriptErrorAppNameKey";
NSString *const OSAScriptErrorRangeKey = @"OSAScriptErrorRangeKey";
NSString *const OSAScriptErrorMessage = @"NSAppleScriptErrorMessage";
NSString *const OSAScriptErrorNumber = @"NSAppleScriptErrorNumber";
NSString *const OSAScriptErrorAppName = @"NSAppleScriptErrorAppName";
NSString *const OSAScriptErrorBriefMessage = @"NSAppleScriptErrorBriefMessage";
NSString *const OSAScriptErrorRange = @"NSAppleScriptErrorRange";

/* errOSASystemError: no scripting component. */
static NSDictionary *unavailable(void) {
    NSString *message = @"There is no scripting component.";
    return @{ OSAScriptErrorMessageKey: message, OSAScriptErrorBriefMessageKey: message, OSAScriptErrorNumberKey: @(-1750),
              OSAScriptErrorRangeKey: [NSValue valueWithRange:NSMakeRange(0, 0)],
              OSAScriptErrorMessage: message, OSAScriptErrorNumber: @(-1750) };
}

@implementation OSALanguage
+ (NSArray *)availableLanguages { return @[]; }
+ (OSALanguage *)languageForName:(NSString *)name { return nil; }
+ (OSALanguage *)defaultLanguage { return nil; }
- (NSString *)name { return nil; }
@end

@implementation OSAScript {
    NSString *_source;
    NSURL *_url;
}

- (instancetype)initWithSource:(NSString *)source {
    return [self initWithSource:source language:nil];
}

- (instancetype)initWithSource:(NSString *)source language:(OSALanguage *)language {
    if ((self = [super init]))
        _source = [source copy] ?: @"";
    return self;
}

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSDictionary<NSString *, id> **)errorInfo {
    NSString *source = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:NULL];
    if (source == nil) {
        if (errorInfo)
            *errorInfo = unavailable();
        return nil;
    }
    if ((self = [self initWithSource:source]))
        _url = [url copy];
    return self;
}

- (id)copyWithZone:(NSZone *)zone { return self; }
- (NSString *)source { return _source; }
- (NSURL *)url { return _url; }
- (OSALanguage *)language { return nil; }
- (BOOL)isCompiled { return NO; }

- (BOOL)compileAndReturnError:(NSDictionary<NSString *, id> **)errorInfo {
    if (errorInfo)
        *errorInfo = unavailable();
    return NO;
}

- (NSAppleEventDescriptor *)executeAndReturnError:(NSDictionary<NSString *, id> **)errorInfo {
    if (errorInfo)
        *errorInfo = unavailable();
    return nil;
}

- (NSAppleEventDescriptor *)executeAppleEvent:(NSAppleEventDescriptor *)event error:(NSDictionary<NSString *, id> **)errorInfo {
    return [self executeAndReturnError:errorInfo];
}

- (NSAppleEventDescriptor *)executeHandlerWithName:(NSString *)name arguments:(NSArray *)arguments error:(NSDictionary<NSString *, id> **)errorInfo {
    return [self executeAndReturnError:errorInfo];
}
@end
