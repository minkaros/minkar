// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The classes of the literals a compiler lays out at build time: @[...],
// @{...} and numbers (NSConstantArray, NSConstantDictionary,
// NSConstantIntegerNumber, NSConstantFloatNumber, NSConstantDoubleNumber),
// and the empty array and dictionary @[] and @{} stand for
// (__NSArray0__struct, __NSDictionary0__struct; __NSArray0__ and
// __NSDictionary0__ for older compilers). The objects live in the binary
// that uses them: their ivars are the compiler's layout after isa, and
// they are never freed.

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#include <limits.h>
#include <string.h>

#define CONSTANT_OBJECT \
   -retain { return self; } \
   -(oneway void)release { } \
   -autorelease { return self; } \
   -(NSUInteger)retainCount { return NSUIntegerMax; } \
   -(void)dealloc { if(0) [super dealloc]; } \
   -copyWithZone:(NSZone *)zone { return self; }

__attribute__((visibility("default")))
@interface NSConstantArray : NSArray {
   NSUInteger _count;
   const id  *_objects;
}
@end

@implementation NSConstantArray
CONSTANT_OBJECT

-(NSUInteger)count {
   return _count;
}

-(id)objectAtIndex:(NSUInteger)index {
   if(index>=_count)
    [NSException raise:NSRangeException format:@"-[NSArray objectAtIndex:]: index %lu beyond bounds [0 .. %ld]",(unsigned long)index,(long)_count-1];
   return _objects[index];
}

-(void)getObjects:(id *)objects range:(NSRange)range {
   if(NSMaxRange(range)>_count)
    [NSException raise:NSRangeException format:@"-[NSArray getObjects:range:]: range {%lu, %lu} beyond bounds [0 .. %ld]",
       (unsigned long)range.location,(unsigned long)range.length,(long)_count-1];
   memcpy(objects,_objects+range.location,range.length*sizeof(id));
}

@end

__attribute__((visibility("default")))
@interface NSConstantDictionary : NSDictionary {
   NSUInteger _options;
   NSUInteger _count;
   const id  *_keys;
   const id  *_objects;
}
@end

@implementation NSConstantDictionary
CONSTANT_OBJECT

-(NSUInteger)count {
   return _count;
}

-(id)objectForKey:(id)key {
   NSUInteger i;

   if(key==nil)
    return nil;
   for(i=0;i<_count;i++)
    if(_keys[i]==key || [_keys[i] isEqual:key])
     return _objects[i];
   return nil;
}

-(NSEnumerator *)keyEnumerator {
   return [[NSArray arrayWithObjects:(id *)_keys count:_count] objectEnumerator];
}

@end

/* Numbers: a signed or unsigned integer of some width (its encoding says
   which), a float or a double. */
__attribute__((visibility("default")))
@interface NSConstantIntegerNumber : NSNumber {
   const char *_encoding;
   long long   _value;
}
@end

@implementation NSConstantIntegerNumber
CONSTANT_OBJECT

static BOOL isUnsigned(const char *encoding) {
   return encoding!=NULL && strchr("CSILQ",encoding[0])!=NULL;
}

-(const char *)objCType {
   return _encoding;
}

-(void)getValue:(void *)value {
   switch(_encoding!=NULL?_encoding[0]:'q'){
    case 'c': case 'B': *(char *)value=(char)_value; break;
    case 'C': *(unsigned char *)value=(unsigned char)_value; break;
    case 's': *(short *)value=(short)_value; break;
    case 'S': *(unsigned short *)value=(unsigned short)_value; break;
    case 'i': *(int *)value=(int)_value; break;
    case 'I': *(unsigned int *)value=(unsigned int)_value; break;
    case 'l': *(long *)value=(long)_value; break;
    case 'L': *(unsigned long *)value=(unsigned long)_value; break;
    case 'Q': *(unsigned long long *)value=(unsigned long long)_value; break;
    default: *(long long *)value=_value; break;
   }
}

-(char)charValue { return (char)_value; }
-(unsigned char)unsignedCharValue { return (unsigned char)_value; }
-(short)shortValue { return (short)_value; }
-(unsigned short)unsignedShortValue { return (unsigned short)_value; }
-(int)intValue { return (int)_value; }
-(unsigned int)unsignedIntValue { return (unsigned int)_value; }
-(long)longValue { return (long)_value; }
-(unsigned long)unsignedLongValue { return (unsigned long)_value; }
-(long long)longLongValue { return _value; }
-(unsigned long long)unsignedLongLongValue { return (unsigned long long)_value; }
-(NSInteger)integerValue { return (NSInteger)_value; }
-(NSUInteger)unsignedIntegerValue { return (NSUInteger)_value; }
-(float)floatValue { return isUnsigned(_encoding)?(float)(unsigned long long)_value:(float)_value; }
-(double)doubleValue { return isUnsigned(_encoding)?(double)(unsigned long long)_value:(double)_value; }
-(BOOL)boolValue { return _value!=0; }

-(NSString *)descriptionWithLocale:(id)locale {
   if(isUnsigned(_encoding))
    return [NSString stringWithFormat:@"%llu",(unsigned long long)_value];
   return [NSString stringWithFormat:@"%lld",_value];
}

@end

__attribute__((visibility("default")))
@interface NSConstantFloatNumber : NSNumber {
   float _value;
}
@end

__attribute__((visibility("default")))
@interface NSConstantDoubleNumber : NSNumber {
   double _value;
}
@end

#define FLOATING_NUMBER(type,encoding) \
   CONSTANT_OBJECT \
   -(const char *)objCType { return encoding; } \
   -(void)getValue:(void *)value { *(type *)value=_value; } \
   -(char)charValue { return (char)_value; } \
   -(unsigned char)unsignedCharValue { return (unsigned char)_value; } \
   -(short)shortValue { return (short)_value; } \
   -(unsigned short)unsignedShortValue { return (unsigned short)_value; } \
   -(int)intValue { return (int)_value; } \
   -(unsigned int)unsignedIntValue { return (unsigned int)_value; } \
   -(long)longValue { return (long)_value; } \
   -(unsigned long)unsignedLongValue { return (unsigned long)_value; } \
   -(long long)longLongValue { return (long long)_value; } \
   -(unsigned long long)unsignedLongLongValue { return (unsigned long long)_value; } \
   -(NSInteger)integerValue { return (NSInteger)_value; } \
   -(NSUInteger)unsignedIntegerValue { return (NSUInteger)_value; } \
   -(float)floatValue { return (float)_value; } \
   -(double)doubleValue { return (double)_value; } \
   -(BOOL)boolValue { return _value!=0; } \
   -(NSString *)descriptionWithLocale:(id)locale { return [NSString stringWithFormat:@"%0.*g",(int)(sizeof(type)==4?7:17),(double)_value]; }

@implementation NSConstantFloatNumber
FLOATING_NUMBER(float,"f")
@end

@implementation NSConstantDoubleNumber
FLOATING_NUMBER(double,"d")
@end

/* The empty collections, laid out as the constant classes are. */
struct NSConstantArrayLayout { Class isa; NSUInteger count; const id *objects; };
struct NSConstantDictionaryLayout { Class isa; NSUInteger options; NSUInteger count; const id *keys; const id *objects; };

extern char NSConstantArrayClass __asm__("_OBJC_CLASS_$_NSConstantArray");
extern char NSConstantDictionaryClass __asm__("_OBJC_CLASS_$_NSConstantDictionary");

__attribute__((visibility("default"))) struct NSConstantArrayLayout __NSArray0__struct = { (Class)&NSConstantArrayClass, 0, NULL };
__attribute__((visibility("default"))) struct NSConstantDictionaryLayout __NSDictionary0__struct = { (Class)&NSConstantDictionaryClass, 0, 0, NULL, NULL };

__attribute__((visibility("default"))) id __NSArray0__ = (id)&__NSArray0__struct;
__attribute__((visibility("default"))) id __NSDictionary0__ = (id)&__NSDictionary0__struct;
