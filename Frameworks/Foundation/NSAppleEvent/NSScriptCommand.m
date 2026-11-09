// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptCommand.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSThread.h>

static NSString *const kCurrentCommandKey=@"NSScriptCommandCurrent";

@implementation NSScriptCommand

@synthesize commandDescription=_commandDescription, directParameter=_directParameter, receiversSpecifier=_receiversSpecifier,
   arguments=_arguments, scriptErrorNumber=_errorNumber, scriptErrorString=_errorString,
   scriptErrorExpectedTypeDescriptor=_errorExpectedType, appleEvent=_appleEvent;

-init {
   return [self initWithCommandDescription:nil];
}

-initWithCommandDescription:(id)commandDef {
   self=[super init];
   if(self!=nil)
    _commandDescription=[commandDef retain];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   self=[super init];
   if(self!=nil){
    _arguments=[[coder decodeObjectForKey:@"NSArguments"] copy];
    _directParameter=[[coder decodeObjectForKey:@"NSDirectParameter"] retain];
   }
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_arguments forKey:@"NSArguments"];
   [coder encodeObject:_directParameter forKey:@"NSDirectParameter"];
}

-(void)dealloc {
   [_commandDescription release];
   [_appleEvent release];
   [_arguments release];
   [_directParameter release];
   [_receiversSpecifier release];
   [_errorString release];
   [_errorExpectedType release];
   [super dealloc];
}

-(id)evaluatedReceivers {
   return _receiversSpecifier;
}

-(NSDictionary *)evaluatedArguments {
   return _arguments;
}

-(BOOL)isWellFormed {
   return YES;
}

-(NSAppleEventDescriptor *)scriptErrorOffendingObjectDescriptor {
   return nil;
}

-(void)setScriptErrorOffendingObjectDescriptor:(NSAppleEventDescriptor *)descriptor {
}

-(id)performDefaultImplementation {
   return nil;
}

+(NSScriptCommand *)currentCommand {
   return [[[NSThread currentThread] threadDictionary] objectForKey:kCurrentCommandKey];
}

-(id)executeCommand {
   NSMutableDictionary *thread=[[NSThread currentThread] threadDictionary];
   id                   previous=[[thread objectForKey:kCurrentCommandKey] retain];
   id                   result;

   [thread setObject:self forKey:kCurrentCommandKey];
   result=[[self performDefaultImplementation] retain];
   if(previous!=nil)
    [thread setObject:previous forKey:kCurrentCommandKey];
   else
    [thread removeObjectForKey:kCurrentCommandKey];
   [previous release];
   return [result autorelease];
}

-(void)suspendExecution {
   _suspended=YES;
}

-(void)resumeExecutionWithResult:(id)result {
   _suspended=NO;
}

@end
