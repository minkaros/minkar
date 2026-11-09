// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSAppleScript.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSRange.h>

NSString *const NSAppleScriptErrorMessage=@"NSAppleScriptErrorMessage";
NSString *const NSAppleScriptErrorNumber=@"NSAppleScriptErrorNumber";
NSString *const NSAppleScriptErrorAppName=@"NSAppleScriptErrorAppName";
NSString *const NSAppleScriptErrorBriefMessage=@"NSAppleScriptErrorBriefMessage";
NSString *const NSAppleScriptErrorRange=@"NSAppleScriptErrorRange";

/* errOSASystemError: no scripting component to run the script. */
enum { kNoScriptingComponent=-1750 };

static NSDictionary *unavailable(void) {
   NSString *message=@"AppleScript is not available.";
   return [NSDictionary dictionaryWithObjectsAndKeys:
      message,NSAppleScriptErrorMessage,
      message,NSAppleScriptErrorBriefMessage,
      [NSNumber numberWithInt:kNoScriptingComponent],NSAppleScriptErrorNumber,
      [NSValue valueWithRange:NSMakeRange(0,0)],NSAppleScriptErrorRange,
      nil];
}

@implementation NSAppleScript

-init {
   return [self initWithSource:@""];
}

-initWithSource:(NSString *)source {
   self=[super init];
   if(self!=nil)
    _source=[source copy];
   return self;
}

-initWithContentsOfURL:(NSURL *)url error:(NSDictionary **)errorInfo {
   NSString *source=[NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:NULL];
   if(source==nil){
    /* A compiled script: there is nothing to read it with. */
    if(errorInfo!=NULL)
     *errorInfo=unavailable();
    [self release];
    return nil;
   }
   self=[self initWithSource:source];
   if(self!=nil)
    _url=[url copy];
   return self;
}

-(void)dealloc {
   [_source release];
   [_url release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-(NSString *)source {
   return _source;
}

-(BOOL)isCompiled {
   return NO;
}

-(BOOL)compileAndReturnError:(NSDictionary **)errorInfo {
   if(errorInfo!=NULL)
    *errorInfo=unavailable();
   return NO;
}

-(NSAppleEventDescriptor *)executeAndReturnError:(NSDictionary **)errorInfo {
   if(errorInfo!=NULL)
    *errorInfo=unavailable();
   return nil;
}

-(NSAppleEventDescriptor *)executeAppleEvent:(NSAppleEventDescriptor *)event error:(NSDictionary **)errorInfo {
   return [self executeAndReturnError:errorInfo];
}

@end
