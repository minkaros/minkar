// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSDictionary.h>

@class NSAppleEventDescriptor, NSDictionary, NSString, NSCoder;

NS_ASSUME_NONNULL_BEGIN

enum {
    NSNoScriptError = 0,
    NSReceiverEvaluationScriptError,
    NSKeySpecifierEvaluationScriptError,
    NSArgumentEvaluationScriptError,
    NSReceiversCantHandleCommandScriptError,
    NSRequiredArgumentsMissingScriptError,
    NSArgumentsWrongScriptError,
    NSUnknownKeyScriptError,
    NSInternalScriptError,
    NSOperationNotSupportedForKeyScriptError,
    NSCannotCreateScriptCommandError
};

/* A command from a script, run by executeCommand: subclasses do the work
   in performDefaultImplementation. Commands are made by the scripting
   machinery from Apple events; there is no scripting dictionary to make
   them from yet, so they come from subclasses and tests. */
@interface NSScriptCommand : NSObject <NSCoding> {
    id _commandDescription;
    NSAppleEventDescriptor *_appleEvent;
    NSDictionary *_arguments;
    id _directParameter;
    id _receiversSpecifier;
    NSInteger _errorNumber;
    NSString *_errorString;
    id _errorExpectedType;
    BOOL _suspended;
}

- (instancetype)initWithCommandDescription:(nullable id)commandDef NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property (nullable, readonly, retain) id commandDescription;
@property (nullable, retain) id directParameter;
@property (nullable, retain) id receiversSpecifier;
@property (nullable, readonly, retain) id evaluatedReceivers;
@property (nullable, copy) NSDictionary<NSString *, id> *arguments;
@property (nullable, readonly, copy) NSDictionary<NSString *, id> *evaluatedArguments;
@property (readonly, getter=isWellFormed) BOOL wellFormed;
@property NSInteger scriptErrorNumber;
@property (nullable, retain) NSAppleEventDescriptor *scriptErrorOffendingObjectDescriptor;
@property (nullable, retain) NSAppleEventDescriptor *scriptErrorExpectedTypeDescriptor;
@property (nullable, copy) NSString *scriptErrorString;
@property (nullable, readonly, retain) NSAppleEventDescriptor *appleEvent;

- (nullable id)performDefaultImplementation;
- (nullable id)executeCommand;
+ (nullable NSScriptCommand *)currentCommand;
- (void)suspendExecution;
- (void)resumeExecutionWithResult:(nullable id)result;

@end

NS_ASSUME_NONNULL_END
