/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSMutableDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSCoder.h>

NSString * const NSPOSIXErrorDomain=@"NSPOSIXErrorDomain";
NSString * const NSOSStatusErrorDomain=@"NSOSStatusErrorDomain";
NSString * const NSWINSOCKErrorDomain=@"NSWINSOCKErrorDomain";
NSString * const NSCocoaErrorDomain=@"NSCocoaErrorDomain";

NSString * const NSUnderlyingErrorKey=@"NSUnderlyingErrorKey";
NSString * const NSLocalizedDescriptionKey=@"NSLocalizedDescriptionKey";
NSString * const NSLocalizedFailureReasonErrorKey=@"NSLocalizedFailureReasonErrorKey";
NSString * const NSLocalizedRecoveryOptionsErrorKey=@"NSLocalizedRecoveryOptionsErrorKey";
NSString * const NSLocalizedRecoverySuggestionErrorKey=@"NSLocalizedRecoverySuggestionErrorKey";
// Minkar: declared in NSError.h, never defined (a program using one did not link).
NSString * const NSStringEncodingErrorKey=@"NSStringEncodingErrorKey";
NSString * const NSFilePathErrorKey=@"NSFilePath";
NSString * const NSErrorFailingURLStringKey=@"NSErrorFailingURLStringKey";
NSString * const NSURLErrorKey=@"NSURL";
NSString * const NSRecoveryAttempterErrorKey=@"NSRecoveryAttempterErrorKey";
NSString * const NSDebugDescriptionErrorKey=@"NSDebugDescription";
NSString * const NSMachErrorDomain=@"NSMachErrorDomain";
NSString * const NSHelpAnchorErrorKey=@"NSHelpAnchor";
NSString * const NSLocalizedFailureErrorKey=@"NSLocalizedFailure";
NSString * const NSMultipleUnderlyingErrorsKey=@"NSMultipleUnderlyingErrorsKey";

/* The blocks that give an error's values its user info lacks, by domain. */
static NSMutableDictionary *valueProviders;

@implementation NSError

-initWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo {
   _domain=[domain copy];
   _code=code;
   _userInfo=[userInfo retain];
   return self;
}

-(void)dealloc {
   [_domain release];
   [_userInfo release];
   [super dealloc];
}

/* Minkar: errors are archived (and sent over NSXPCConnection) with their
 * domain, code and the user info values that can be archived securely
 * (NSSecureCodingSupport.m says NSError supports secure coding). */
static NSDictionary *NSErrorArchivableUserInfo(NSDictionary *userInfo) {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   for(id key in userInfo){
    id    value=[userInfo objectForKey:key];
    Class cls=[value class];

    if(![key isKindOfClass:[NSString class]])
     continue;
    if([cls respondsToSelector:@selector(supportsSecureCoding)] && [cls supportsSecureCoding])
     [result setObject:value forKey:key];
   }
   return result;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSDictionary *userInfo=NSErrorArchivableUserInfo(_userInfo);

   if([coder allowsKeyedCoding]){
    [coder encodeObject:_domain forKey:@"NSDomain"];
    [coder encodeInteger:_code forKey:@"NSCode"];
    if([userInfo count]>0)
     [coder encodeObject:userInfo forKey:@"NSUserInfo"];
   }
   else {
    NSInteger code=_code;

    [coder encodeObject:_domain];
    [coder encodeValueOfObjCType:@encode(NSInteger) at:&code];
    [coder encodeObject:userInfo];
   }
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSSet *values=[NSSet setWithObjects:[NSDictionary class],[NSArray class],[NSString class],
      [NSNumber class],[NSData class],[NSDate class],[NSURL class],[NSError class],[NSNull class],nil];

    _domain=[[coder decodeObjectOfClass:[NSString class] forKey:@"NSDomain"] copy];
    _code=[coder decodeIntegerForKey:@"NSCode"];
    _userInfo=[[coder decodeObjectOfClasses:values forKey:@"NSUserInfo"] retain];
   }
   else {
    _domain=[[coder decodeObject] copy];
    [coder decodeValueOfObjCType:@encode(NSInteger) at:&_code];
    _userInfo=[[coder decodeObject] retain];
   }
   if(_domain==nil){
    [self release];
    return nil;
   }
   return self;
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

+errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo {
   return [[[self alloc] initWithDomain:domain code:code userInfo:userInfo] autorelease];
}

-(NSString *)domain {
   return _domain;
}

-(NSInteger)code {
   return _code;
}

-(NSDictionary *)userInfo {
   return _userInfo!=nil ? _userInfo : [NSDictionary dictionary];
}

+(void)setUserInfoValueProviderForDomain:(NSString *)domain provider:(id (^)(NSError *, NSString *))provider {
   @synchronized(self){
    if(valueProviders==nil)
     valueProviders=[[NSMutableDictionary alloc] init];
    if(provider==nil)
     [valueProviders removeObjectForKey:domain];
    else {
     id copy=[provider copy];
     [valueProviders setObject:copy forKey:domain];
     [copy release];
    }
   }
}

+(id (^)(NSError *, NSString *))userInfoValueProviderForDomain:(NSString *)domain {
   id provider;

   @synchronized(self){
    provider=[[[valueProviders objectForKey:domain] retain] autorelease];
   }
   return provider;
}

/* A value of the user info's, else its domain's provider's. Through the
 * accessors: an error bridged from Swift has its own. */
-(id)_valueForUserInfoKey:(NSString *)key {
   id value=[[self userInfo] objectForKey:key];

   if(value==nil){
    id (^provider)(NSError *, NSString *)=[NSError userInfoValueProviderForDomain:[self domain]];
    if(provider!=nil)
     value=provider(self,key);
   }
   return value;
}

-(NSString *)helpAnchor {
   return [self _valueForUserInfoKey:NSHelpAnchorErrorKey];
}

-(NSArray *)underlyingErrors {
   NSMutableArray *errors=[NSMutableArray array];
   id underlying=[[self userInfo] objectForKey:NSUnderlyingErrorKey];
   id multiple=[[self userInfo] objectForKey:NSMultipleUnderlyingErrorsKey];

   if([underlying isKindOfClass:[NSError class]])
    [errors addObject:underlying];
   if([multiple isKindOfClass:[NSArray class]])
    [errors addObjectsFromArray:multiple];
   return errors;
}

-(NSString *)localizedDescription {
   NSString *localizedDescription;

   localizedDescription = [self _valueForUserInfoKey:NSLocalizedDescriptionKey];
   if (localizedDescription != nil)
      return localizedDescription;

    localizedDescription = [self localizedFailureReason];
    if (localizedDescription) {
        return [NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"The operation could not be completed.", @"Default NSError description"), localizedDescription];        
    } else {
        return [NSString stringWithFormat:@"%@ (%@ error %ld)", NSLocalizedString(@"The operation could not be completed.", @"Default NSError description"), [self domain], (long)[self code]];
    }
}

-(NSString *)localizedFailureReason {
   return [self _valueForUserInfoKey:NSLocalizedFailureReasonErrorKey];
}

-(NSArray *)localizedRecoveryOptions {
   return [self _valueForUserInfoKey:NSLocalizedRecoveryOptionsErrorKey];
}

-(NSString *)localizedRecoverySuggestion {
   return [self _valueForUserInfoKey:NSLocalizedRecoverySuggestionErrorKey];
}

-(id)recoveryAttempter {
   return [self _valueForUserInfoKey:NSRecoveryAttempterErrorKey];
}

-(id)description {
   NSDictionary *userInfo=[self userInfo];

   if([userInfo count]==0)
    return [NSString stringWithFormat:@"Error Domain=%@ Code=%ld \"%@\"", [self domain], (long)[self code], [self localizedDescription]];
   return [NSString stringWithFormat:@"Error Domain=%@ Code=%ld \"%@\" UserInfo=%@", [self domain], (long)[self code], [self localizedDescription], userInfo];
}

@end
