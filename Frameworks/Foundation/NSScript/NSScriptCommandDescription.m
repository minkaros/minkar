// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptCommandDescription.h>
#import <Foundation/NSScriptCommand.h>
#import <Foundation/Foundation.h>
#import "NSScriptCodes.h"

/* A command declaration of a suite: CommandClass, AppleEventClassCode,
   AppleEventCode, Arguments (each with Type, AppleEventCode, Optional),
   Type and ResultAppleEventCode. */
@implementation NSScriptCommandDescription {
    NSString *_suiteName;
    NSString *_commandName;
    NSDictionary *_declaration;
}

- (instancetype)initWithSuiteName:(NSString *)suiteName commandName:(NSString *)commandName dictionary:(NSDictionary *)commandDeclaration {
    if (suiteName == nil || commandName == nil)
        return nil;
    if ((self = [super init]) != nil) {
        _suiteName = [suiteName copy];
        _commandName = [commandName copy];
        _declaration = [commandDeclaration copy] ?: @{};
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithSuiteName:[coder decodeObjectForKey:@"NSSuiteName"] commandName:[coder decodeObjectForKey:@"NSCommandName"]
                        dictionary:[coder decodeObjectForKey:@"NSDeclaration"]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_suiteName forKey:@"NSSuiteName"];
    [coder encodeObject:_commandName forKey:@"NSCommandName"];
    [coder encodeObject:_declaration forKey:@"NSDeclaration"];
}

- (NSString *)suiteName { return _suiteName; }
- (NSString *)commandName { return _commandName; }

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %@ (%@)>", NSStringFromClass([self class]), _commandName, _suiteName];
}

- (FourCharCode)appleEventClassCode {
    return NSScriptFourCharCode(_declaration[@"AppleEventClassCode"]);
}

- (FourCharCode)appleEventCode {
    return NSScriptFourCharCode(_declaration[@"AppleEventCode"]);
}

- (NSString *)commandClassName {
    return _declaration[@"CommandClass"] ?: @"NSScriptCommand";
}

- (NSString *)returnType {
    return _declaration[@"Type"];
}

- (FourCharCode)appleEventCodeForReturnType {
    return NSScriptFourCharCode(_declaration[@"ResultAppleEventCode"]);
}

- (NSArray<NSString *> *)argumentNames {
    return [[_declaration[@"Arguments"] allKeys] sortedArrayUsingSelector:@selector(compare:)] ?: @[];
}

- (NSString *)typeForArgumentWithName:(NSString *)argumentName {
    return _declaration[@"Arguments"][argumentName][@"Type"];
}

- (FourCharCode)appleEventCodeForArgumentWithName:(NSString *)argumentName {
    return NSScriptFourCharCode(_declaration[@"Arguments"][argumentName][@"AppleEventCode"]);
}

- (BOOL)isOptionalArgumentWithName:(NSString *)argumentName {
    return [_declaration[@"Arguments"][argumentName][@"Optional"] boolValue];
}

/* An instance of the command's class (NSScriptCommand without one). */
- (NSScriptCommand *)createCommandInstanceWithZone:(NSZone *)zone {
    Class commandClass = NSClassFromString(self.commandClassName);
    if (commandClass == Nil || ![commandClass isSubclassOfClass:[NSScriptCommand class]])
        commandClass = [NSScriptCommand class];
    return [[commandClass allocWithZone:zone] initWithCommandDescription:self];
}

- (NSScriptCommand *)createCommandInstance {
    return [self createCommandInstanceWithZone:NULL];
}

@end
