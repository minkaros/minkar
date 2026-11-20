// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptSuiteRegistry.h>
#import <Foundation/NSScriptClassDescription.h>
#import <Foundation/NSScriptCommandDescription.h>
#import <Foundation/Foundation.h>
#import "NSScriptCodes.h"
#import "NSScriptDefinitionReader.h"

/* A suite: its code, bundle, classes and commands. */
@interface _NSScriptSuite : NSObject
@property (nonatomic) FourCharCode appleEventCode;
@property (nonatomic, strong) NSBundle *bundle;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSScriptClassDescription *> *classes;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSScriptCommandDescription *> *commands;
@end

@implementation _NSScriptSuite
@end

@implementation NSScriptSuiteRegistry {
    NSMutableArray<NSString *> *_suiteNames;   /* in the order loaded */
    NSMutableDictionary<NSString *, _NSScriptSuite *> *_suites;
}

static NSScriptSuiteRegistry *sharedRegistry;

/* The standard suites (Foundation's), then the main bundle's: its
   scripting definition (an sdef) and .scriptSuite files. */
+ (NSScriptSuiteRegistry *)sharedScriptSuiteRegistry {
    @synchronized(self) {
        if (sharedRegistry == nil) {
            sharedRegistry = [[self alloc] init];
            NSBundle *foundation = [NSBundle bundleForClass:[NSScriptSuiteRegistry class]];
            for (NSString *name in @[ @"NSCoreSuite", @"NSTextSuite" ]) {
                NSString *path = [foundation pathForResource:name ofType:@"plist"];
                NSDictionary *suite = path ? [NSDictionary dictionaryWithContentsOfFile:path] : nil;
                if (suite != nil)
                    [sharedRegistry loadSuiteWithDictionary:suite fromBundle:foundation];
            }
            NSBundle *main = [NSBundle mainBundle];
            if (main != nil) {
                [sharedRegistry _loadScriptingDefinitionOfBundle:main];
                [sharedRegistry loadSuitesFromBundle:main];
            }
        }
        return sharedRegistry;
    }
}

+ (void)setSharedScriptSuiteRegistry:(NSScriptSuiteRegistry *)registry {
    @synchronized(self) {
        sharedRegistry = registry;
    }
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _suiteNames = [NSMutableArray array];
        _suites = [NSMutableDictionary dictionary];
    }
    return self;
}

- (_NSScriptSuite *)_suiteNamed:(NSString *)name bundle:(NSBundle *)bundle {
    _NSScriptSuite *suite = _suites[name];
    if (suite == nil) {
        suite = [_NSScriptSuite new];
        suite.classes = [NSMutableDictionary dictionary];
        suite.commands = [NSMutableDictionary dictionary];
        suite.bundle = bundle;
        _suites[name] = suite;
        [_suiteNames addObject:name];
    }
    return suite;
}

#pragma mark Loading

/* The bundle's .scriptSuite files (the dictionary format). */
- (void)loadSuitesFromBundle:(NSBundle *)bundle {
    for (NSString *path in [bundle pathsForResourcesOfType:@"scriptSuite" inDirectory:nil]) {
        NSDictionary *suite = [NSDictionary dictionaryWithContentsOfFile:path];
        if (suite != nil)
            [self loadSuiteWithDictionary:suite fromBundle:bundle];
    }
}

/* A bundle's sdef, as its Info.plist names it (OSAScriptingDefinition),
   made into suites of the dictionary format. */
- (void)_loadScriptingDefinitionOfBundle:(NSBundle *)bundle {
    NSString *name = [bundle objectForInfoDictionaryKey:@"OSAScriptingDefinition"];
    if (![name isKindOfClass:[NSString class]] || name.length == 0)
        return;
    NSString *path = [bundle pathForResource:name.stringByDeletingPathExtension ofType:name.pathExtension.length ? name.pathExtension : @"sdef"];
    NSData *data = path ? [NSData dataWithContentsOfFile:path] : nil;
    for (NSDictionary *suite in NSScriptSuitesFromDefinition(data))
        [self loadSuiteWithDictionary:suite fromBundle:bundle];
}

- (void)loadSuiteWithDictionary:(NSDictionary *)declaration fromBundle:(NSBundle *)bundle {
    NSString *suiteName = declaration[@"Name"];
    if (![suiteName isKindOfClass:[NSString class]])
        return;
    @synchronized(self) {
        _NSScriptSuite *suite = [self _suiteNamed:suiteName bundle:bundle];
        if (declaration[@"AppleEventCode"] != nil)
            suite.appleEventCode = NSScriptFourCharCode(declaration[@"AppleEventCode"]);
        NSDictionary *classes = declaration[@"Classes"];
        for (NSString *className in classes) {
            NSScriptClassDescription *description = [[NSScriptClassDescription alloc] initWithSuiteName:suiteName className:className dictionary:classes[className]];
            if (description != nil)
                suite.classes[className] = description;
        }
        NSDictionary *commands = declaration[@"Commands"];
        for (NSString *commandName in commands) {
            NSScriptCommandDescription *description = [[NSScriptCommandDescription alloc] initWithSuiteName:suiteName commandName:commandName dictionary:commands[commandName]];
            if (description != nil)
                suite.commands[commandName] = description;
        }
    }
}

- (void)registerClassDescription:(NSScriptClassDescription *)classDescription {
    if (classDescription.suiteName == nil || classDescription.className == nil)
        return;
    @synchronized(self) {
        [self _suiteNamed:classDescription.suiteName bundle:nil].classes[classDescription.className] = classDescription;
    }
}

- (void)registerCommandDescription:(NSScriptCommandDescription *)commandDescription {
    @synchronized(self) {
        [self _suiteNamed:commandDescription.suiteName bundle:nil].commands[commandDescription.commandName] = commandDescription;
    }
}

#pragma mark Looking up

- (NSArray<NSString *> *)suiteNames {
    @synchronized(self) {
        return [_suiteNames copy];
    }
}

- (FourCharCode)appleEventCodeForSuite:(NSString *)suiteName {
    @synchronized(self) {
        _NSScriptSuite *suite = _suites[suiteName];
        return suite ? (suite.appleEventCode ?: '????') : 0;
    }
}

- (NSBundle *)bundleForSuite:(NSString *)suiteName {
    @synchronized(self) {
        return _suites[suiteName].bundle;
    }
}

- (NSDictionary<NSString *, NSScriptClassDescription *> *)classDescriptionsInSuite:(NSString *)suiteName {
    @synchronized(self) {
        return [_suites[suiteName].classes copy];
    }
}

- (NSDictionary<NSString *, NSScriptCommandDescription *> *)commandDescriptionsInSuite:(NSString *)suiteName {
    @synchronized(self) {
        return [_suites[suiteName].commands copy];
    }
}

- (NSString *)suiteForAppleEventCode:(FourCharCode)appleEventCode {
    @synchronized(self) {
        for (NSString *name in _suiteNames)
            if (_suites[name].appleEventCode == appleEventCode)
                return name;
        return nil;
    }
}

- (NSScriptClassDescription *)classDescriptionWithAppleEventCode:(FourCharCode)appleEventCode {
    @synchronized(self) {
        for (NSString *name in _suiteNames.reverseObjectEnumerator)
            for (NSScriptClassDescription *description in _suites[name].classes.allValues)
                if (description.appleEventCode == appleEventCode)
                    return description;
        return nil;
    }
}

- (NSScriptCommandDescription *)commandDescriptionWithAppleEventClass:(FourCharCode)classCode andAppleEventCode:(FourCharCode)idCode {
    @synchronized(self) {
        for (NSString *name in _suiteNames.reverseObjectEnumerator)
            for (NSScriptCommandDescription *description in _suites[name].commands.allValues)
                if (description.appleEventClassCode == classCode && description.appleEventCode == idCode)
                    return description;
        return nil;
    }
}

/* The old 'aete' terminology resources are not made. */
- (NSData *)aeteResource:(NSString *)languageName {
    return nil;
}

@end
