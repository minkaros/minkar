/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <stdlib.h>
#include <xlocale.h>
#import "NSNumber_double.h"
#import <Foundation/NSStringFormatter.h>

#import "NSNumber_double_const.h"

NSNumber *NSNumber_doubleSpecial(double value)
{
   switch (fpclassify(value))
   {
      case FP_INFINITE:
         return signbit(value)?kNSNumberNegativeInfinity:kNSNumberPositiveInfinity;
      case FP_NAN:
         return kNSNumberNaN;
      case FP_ZERO:
         return signbit(value)?kNSNumberNegativeZero:kNSNumberPositiveZero;
      default:
         if(0) {}  // Without profiling, I'm assuming no one value is more likely than every other value put together, and the compiler will optimize for the first if() branch.
         else if (value == 1.0) return kNSNumberPositiveOne;
         else if (value == -1.0) return kNSNumberNegativeOne;
         return nil;
   }
}

@implementation NSNumber_double

NSNumber *NSNumber_doubleNew(NSZone *zone,double value) {
   NSNumber *result=NSNumber_doubleSpecial(value);
   if (result==nil)
   {
      NSNumber_double *self=NSAllocateObject([NSNumber_double class],0,zone);
       if (self) {
          self->_type=kCFNumberDoubleType;
          self->_value=value;
       }
      result=self;
   }
   return result;
}

-(void)getValue:(void *)value {
   *((double *)value)=_value;
}

-(const char *)objCType {
   return @encode(double);
}

-(char)charValue {
   return _value;
}

-(unsigned char)unsignedCharValue {
   return _value;
}

-(short)shortValue {
   return _value;
}

-(unsigned short)unsignedShortValue {
   return _value;
}

-(int)intValue {
   return (int)_value;
}

-(unsigned int)unsignedIntValue {
   return (unsigned int)_value;
}

-(long)longValue {
   return _value;
}

-(unsigned long)unsignedLongValue {
   return _value;
}

-(long long)longLongValue {
   return _value;
}

-(unsigned long long)unsignedLongLongValue {
   return _value;
}

-(float)floatValue {
   return (float)_value;
}

-(double)doubleValue {
   return _value;
}

-(BOOL)boolValue {
   return _value?YES:NO;
}

-(NSInteger)integerValue {
   return (NSInteger)_value;
}

-(NSUInteger)unsignedIntegerValue {
   return (NSUInteger)_value;
}

// The shortest text that reads back as the same value, as on macOS
// (Minkar: a fixed %0.15g lost digits: 1.0/3 printed 15 of its 16).
-(NSString *)descriptionWithLocale:(NSDictionary *)locale {
   static locale_t cLocale;
   char text[48];

   if(cLocale==NULL)
    cLocale=newlocale(LC_ALL_MASK,"C",NULL);
   for(int precision=1;precision<=17;precision++){
    snprintf_l(text,sizeof(text),cLocale,"%.*g",precision,(double)_value);
    if((double)strtod_l(text,NULL,cLocale)==_value)
     break;
   }
   if(locale==nil)
    return [NSString stringWithUTF8String:text];
   return NSStringWithFormatAndLocale(@"%s",locale,text);
}

@end


@implementation NSNumber_double_const

+ (id) allocWithZone:(NSZone *)zone {
   [NSException raise:NSInternalInconsistencyException format:@"Private class NSNumber_double_const is not intended to be alloced."];
   return nil;
}

-(void)dealloc {
   return;
   [super dealloc];  // Silence compiler warning
}

-(id)retain {
   return self;
}


- (oneway void)release
{
    // will never be released
}


-(id)autorelease {
   return self;
}

-(NSUInteger)retainCount {
   /* "For objects that never get released (that is, their release method
      does nothing), this method should return UINT_MAX, as defined in
      <limits.h>." -- NSObject Protocol Reference
   */
   return UINT_MAX;
}

@end

// This code needs to be here after the @implementation. If it's not clang is generating the NSNumber_double_const
// class reference with a "97" after it. Unknown reasons.

NS_DECLARE_CLASS_SYMBOL(NSNumber_double_const);


// The layout of an NSNumber_double: NSObject's isa, cfinfo and rc (the
// CF-compatible header, 16 bytes), then NSNumber_CF's _type and the value.
// Minkar: without cfinfo and rc, _value sat 8 bytes before where
// -doubleValue reads it, so 0.0, -0.0, 1.0, -1.0, the infinities and NaN
// read back as garbage.
typedef struct
{
    const Class * const isa;
    uint32_t _cfinfo;
    uint32_t _rc;
    CFNumberType _type;
    double _value;
} NSNumber_double_Def;


static const NSNumber_double_Def kPositiveInfinityDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    INFINITY
};


static const NSNumber_double_Def kNegativeInfinityDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    -INFINITY
};


static const NSNumber_double_Def kNaNDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    NAN
};


static const NSNumber_double_Def kPositiveZeroDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    0.0
};


static const NSNumber_double_Def kNegativeZeroDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    -0.0
};


static const NSNumber_double_Def kPositiveOneDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    1.0
};


static const NSNumber_double_Def kNegativeOneDef =
{
    &NS_CLASS_SYMBOL(NSNumber_double_const),
    0, 0,
    kCFNumberDoubleType,
    -1.0
};

const CFNumberRef kCFNumberPositiveInfinity = (CFNumberRef)&kPositiveInfinityDef;
const CFNumberRef kCFNumberNegativeInfinity = (CFNumberRef)&kNegativeInfinityDef;
const CFNumberRef kCFNumberNaN = (CFNumberRef)&kNaNDef;

NS_CONSTOBJ_DEF NSNumber * const kNSNumberPositiveZero = (NSNumber *)&kPositiveZeroDef;
NS_CONSTOBJ_DEF NSNumber * const kNSNumberNegativeZero = (NSNumber *)&kNegativeZeroDef;
NS_CONSTOBJ_DEF NSNumber * const kNSNumberPositiveOne = (NSNumber *)&kPositiveOneDef;
NS_CONSTOBJ_DEF NSNumber * const kNSNumberNegativeOne = (NSNumber *)&kNegativeOneDef;
