/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSScanner.h>
#import <Foundation/NSScanner_concrete.h>
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSZone.h>
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

@implementation NSScanner

+allocWithZone:(NSZone *)zone {
   if(self==[NSScanner class])
    return NSAllocateObject([NSScanner_concrete class],0,NULL);

   return NSAllocateObject(self,0,zone);
}

+scannerWithString:(NSString *)string {
    return [[[self allocWithZone:NULL] initWithString:string] autorelease];
}

+localizedScannerWithString:(NSString *)string {
    NSScanner *scanner = [self scannerWithString:string];

    [scanner setLocale:[NSLocale currentLocale]];

    return scanner;
}

-initWithString:(NSString *)string {
    NSInvalidAbstractInvocation();
    return nil;
}

-copyWithZone:(NSZone *)zone {
   NSScanner *copy=[[NSScanner alloc] initWithString:[self string]];
   
   [copy setCharactersToBeSkipped:[self charactersToBeSkipped]];
   [copy setCaseSensitive:[self caseSensitive]];
   [copy setLocale:[self locale]];
   [copy setScanLocation:[self scanLocation]];
   
   return copy;
}

-(NSString *)string {
    NSInvalidAbstractInvocation();
    return nil;
}

-(NSCharacterSet *)charactersToBeSkipped {
    NSInvalidAbstractInvocation();
    return nil;
}

-(BOOL)caseSensitive {
    NSInvalidAbstractInvocation();
    return NO;
}

-(id)locale {
    NSInvalidAbstractInvocation();
    return nil;
}

-(void)setCharactersToBeSkipped:(NSCharacterSet *)set {
    NSInvalidAbstractInvocation();
}

-(void)setCaseSensitive:(BOOL)flag {
    NSInvalidAbstractInvocation();
}

-(void)setLocale:(id)locale {
    NSInvalidAbstractInvocation();
}

-(BOOL)isAtEnd {
    NSInvalidAbstractInvocation();
    return NO;
}

-(NSUInteger)scanLocation {
    NSInvalidAbstractInvocation();
    return -1;
}

-(void)setScanLocation:(NSUInteger)location {
    NSInvalidAbstractInvocation();
}

-(BOOL)scanInt:(int *)value {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanLongLong:(long long *)value {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanFloat:(float *)value {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanDouble:(double *)value {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanDecimal:(NSDecimal *)valuep {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanInteger:(NSInteger *)valuep {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanHexInt:(unsigned *)value {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanHexLongLong:(unsigned long long *)value {
	NSInvalidAbstractInvocation();
	return NO;
}

-(BOOL)scanString:(NSString *)string intoString:(NSString **)stringp {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanUpToString:(NSString *)string intoString:(NSString **)stringp {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanCharactersFromSet:(NSCharacterSet *)charset intoString:(NSString **)stringp {
    NSInvalidAbstractInvocation();
    return NO;
}

-(BOOL)scanUpToCharactersFromSet:(NSCharacterSet *)charset intoString:(NSString **)stringp {
    NSInvalidAbstractInvocation();
    return NO;
}

/* The characters from the scan location on, after the ones skipped. */
-(NSString *)_restAfterSkipping {
    NSString *string=[self string];
    NSCharacterSet *skip=[self charactersToBeSkipped];
    NSUInteger location=[self scanLocation],length=[string length];

    while(skip!=nil && location<length && [skip characterIsMember:[string characterAtIndex:location]])
     location++;
    [self setScanLocation:location];
    return [string substringFromIndex:location];
}

-(BOOL)scanUnsignedLongLong:(unsigned long long *)result {
    NSUInteger start=[self scanLocation];
    NSString *rest=[self _restAfterSkipping];
    const char *chars=[rest UTF8String];
    char *end=NULL;

    if(chars==NULL || !(chars[0]>='0' && chars[0]<='9')){
     [self setScanLocation:start];
     return NO;
    }
    errno=0;
    unsigned long long value=strtoull(chars,&end,10);
    if(errno==ERANGE)
     value=ULLONG_MAX;
    if(result!=NULL)
     *result=value;
    [self setScanLocation:[self scanLocation]+(NSUInteger)(end-chars)];
    return YES;
}

/* A hexadecimal floating point number: 0x1.8p3 (the 0x is optional). */
-(BOOL)scanHexDouble:(double *)result {
    NSUInteger start=[self scanLocation];
    NSString *rest=[self _restAfterSkipping];
    const char *chars=[rest UTF8String];
    char buffer[128];
    size_t prefix=0,n=0;
    char *end=NULL;

    if(chars==NULL){
     [self setScanLocation:start];
     return NO;
    }
    /* The sign and an optional 0x, then the digits, as strtod reads them. */
    if(chars[0]=='+' || chars[0]=='-')
     buffer[n++]=chars[prefix++];
    if(chars[prefix]=='0' && (chars[prefix+1]=='x' || chars[prefix+1]=='X'))
     prefix+=2;
    buffer[n++]='0';
    buffer[n++]='x';
    strncpy(buffer+n,chars+prefix,sizeof(buffer)-n-1);
    buffer[sizeof(buffer)-1]=0;
    double value=strtod(buffer,&end);
    if(end==buffer+n){
     [self setScanLocation:start];
     return NO;
    }
    if(result!=NULL)
     *result=value;
    [self setScanLocation:[self scanLocation]+prefix+(NSUInteger)(end-(buffer+n))];
    return YES;
}

-(BOOL)scanHexFloat:(float *)result {
    double value;

    if(![self scanHexDouble:&value])
     return NO;
    if(result!=NULL)
     *result=(float)value;
    return YES;
}

@end
