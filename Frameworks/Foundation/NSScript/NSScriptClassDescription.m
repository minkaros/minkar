// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptClassDescription.h>
#import <Foundation/NSScriptCommandDescription.h>
#import <Foundation/NSScriptSuiteRegistry.h>
#import <Foundation/Foundation.h>
#import "NSScriptCodes.h"

/* A class declaration of a suite (the .scriptSuite format): its
   superclass's name, its code, and its Attributes, ToOneRelationships and
   ToManyRelationships, each key with a Type, an AppleEventCode and
   ReadOnly; SupportedCommands maps a command name to a selector. */
@implementation NSScriptClassDescription {
    NSString *_suiteName;
    NSString *_className;
    NSDictionary *_declaration;
    FourCharCode _appleEventCode;
}

/* The description implemented by the class or its nearest superclass:
   the suites loaded last first (an application's over the standard
   ones), and of several for one class (an sdef's "character" and
   "paragraph" are both NSTextStorage) the one not inheriting from
   another of the same class. */
+ (NSScriptClassDescription *)classDescriptionForClass:(Class)aClass {
    NSScriptSuiteRegistry *registry = [NSScriptSuiteRegistry sharedScriptSuiteRegistry];
    NSArray<NSString *> *suites = registry.suiteNames.reverseObjectEnumerator.allObjects;
    for (Class c = aClass; c != Nil; c = [c superclass]) {
        NSString *name = NSStringFromClass(c);
        for (NSString *suite in suites) {
            NSScriptClassDescription *found = nil;
            for (NSScriptClassDescription *description in [registry classDescriptionsInSuite:suite].allValues) {
                if (![description.implementationClassName isEqualToString:name])
                    continue;
                if (found == nil || [found.superclassDescription.implementationClassName isEqualToString:name])
                    found = description;
            }
            if (found != nil)
                return found;
        }
    }
    return nil;
}

- (instancetype)initWithSuiteName:(NSString *)suiteName className:(NSString *)className dictionary:(NSDictionary *)classDeclaration {
    if (className == nil)
        return nil;
    if ((self = [super init]) != nil) {
        _suiteName = [suiteName copy];
        _className = [className copy];
        _declaration = [classDeclaration copy] ?: @{};
        _appleEventCode = NSScriptFourCharCode(_declaration[@"AppleEventCode"]);
    }
    return self;
}

- (NSString *)suiteName { return _suiteName; }
- (NSString *)className { return _className; }
- (FourCharCode)appleEventCode { return _appleEventCode; }

- (NSString *)implementationClassName {
    return _declaration[@"ImplementationClassName"] ?: _className;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %@ (%@)>", NSStringFromClass([self class]), _className, _suiteName];
}

/* Found by name in the registry when asked for. */
- (NSScriptClassDescription *)superclassDescription {
    NSString *name = _declaration[@"Superclass"];
    if (name == nil)
        return nil;
    NSScriptSuiteRegistry *registry = [NSScriptSuiteRegistry sharedScriptSuiteRegistry];
    NSScriptClassDescription *found = [registry classDescriptionsInSuite:_suiteName][name];
    if (found != nil)
        return found;
    for (NSString *suite in registry.suiteNames)
        if ((found = [registry classDescriptionsInSuite:suite][name]) != nil)
            return found;
    return nil;
}

- (BOOL)matchesAppleEventCode:(FourCharCode)appleEventCode {
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription)
        if (d->_appleEventCode == appleEventCode)
            return YES;
    return NO;
}

#pragma mark Keys

static NSArray<NSString *> *const sections(void) {
    return @[ @"Attributes", @"ToOneRelationships", @"ToManyRelationships" ];
}

/* A key's declaration and its section, here or in a superclass. */
- (NSDictionary *)_declarationForKey:(NSString *)key section:(NSString **)section {
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription)
        for (NSString *name in sections()) {
            NSDictionary *entry = d->_declaration[name][key];
            if ([entry isKindOfClass:[NSDictionary class]]) {
                if (section)
                    *section = name;
                return entry;
            }
        }
    return nil;
}

- (NSArray *)_keysInSection:(NSString *)section {
    NSMutableArray *keys = [NSMutableArray array];
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription)
        for (NSString *key in d->_declaration[section])
            if (![keys containsObject:key])
                [keys addObject:key];
    return keys;
}

- (NSArray *)attributeKeys { return [self _keysInSection:@"Attributes"]; }
- (NSArray *)toOneRelationshipKeys { return [self _keysInSection:@"ToOneRelationships"]; }
- (NSArray *)toManyRelationshipKeys { return [self _keysInSection:@"ToManyRelationships"]; }

- (NSString *)typeForKey:(NSString *)key {
    return [self _declarationForKey:key section:NULL][@"Type"];
}

- (NSScriptClassDescription *)classDescriptionForKey:(NSString *)key {
    NSString *type = [self typeForKey:key];
    if (type == nil)
        return nil;
    NSRange angle = [type rangeOfString:@"<"];
    if (angle.location != NSNotFound)
        type = [type substringToIndex:angle.location];
    NSScriptSuiteRegistry *registry = [NSScriptSuiteRegistry sharedScriptSuiteRegistry];
    for (NSString *suite in registry.suiteNames) {
        NSScriptClassDescription *found = [registry classDescriptionsInSuite:suite][type];
        if (found != nil)
            return found;
    }
    return nil;
}

- (FourCharCode)appleEventCodeForKey:(NSString *)key {
    return NSScriptFourCharCode([self _declarationForKey:key section:NULL][@"AppleEventCode"]);
}

- (NSString *)keyWithAppleEventCode:(FourCharCode)appleEventCode {
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription)
        for (NSString *name in sections()) {
            NSDictionary *keys = d->_declaration[name];
            for (NSString *key in keys)
                if (NSScriptFourCharCode(keys[key][@"AppleEventCode"]) == appleEventCode)
                    return key;
        }
    return nil;
}

- (NSString *)defaultSubcontainerAttributeKey {
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription)
        if (d->_declaration[@"DefaultSubcontainerAttribute"] != nil)
            return d->_declaration[@"DefaultSubcontainerAttribute"];
    return nil;
}

- (BOOL)isLocationRequiredToCreateForKey:(NSString *)key {
    id value = [self _declarationForKey:key section:NULL][@"LocationRequiredToCreate"];
    return value == nil ? YES : [value boolValue];
}

- (BOOL)hasPropertyForKey:(NSString *)key {
    NSString *section = nil;
    return [self _declarationForKey:key section:&section] != nil && ![section isEqualToString:@"ToManyRelationships"];
}

- (BOOL)hasOrderedToManyRelationshipForKey:(NSString *)key {
    NSString *section = nil;
    return [self _declarationForKey:key section:&section] != nil && [section isEqualToString:@"ToManyRelationships"];
}

- (BOOL)hasReadablePropertyForKey:(NSString *)key {
    return [self _declarationForKey:key section:NULL] != nil;
}

- (BOOL)hasWritablePropertyForKey:(NSString *)key {
    NSDictionary *entry = [self _declarationForKey:key section:NULL];
    return entry != nil && ![entry[@"ReadOnly"] boolValue];
}

- (BOOL)isReadOnlyKey:(NSString *)key {
    return ![self hasWritablePropertyForKey:key];
}

#pragma mark Commands

- (BOOL)supportsCommand:(NSScriptCommandDescription *)commandDescription {
    return [self selectorForCommand:commandDescription] != NULL;
}

- (SEL)selectorForCommand:(NSScriptCommandDescription *)commandDescription {
    for (NSScriptClassDescription *d = self; d != nil; d = d.superclassDescription) {
        NSString *selector = d->_declaration[@"SupportedCommands"][commandDescription.commandName];
        if (selector == nil)
            selector = d->_declaration[@"SupportedCommands"][[NSString stringWithFormat:@"%@.%@", commandDescription.suiteName, commandDescription.commandName]];
        if (selector.length > 0)
            return NSSelectorFromString(selector);
    }
    return NULL;
}

@end

@implementation NSObject (NSScriptClassDescription)

- (FourCharCode)classCode {
    return [NSScriptClassDescription classDescriptionForClass:[self class]].appleEventCode;
}

@end
