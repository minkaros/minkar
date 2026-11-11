/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#include <sys/param.h>
#import <objc/runtime.h>
#import <Foundation/NSAttributedString.h>
#import <Foundation/NSMutableAttributedString.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSRaise.h>

#import "NSAttributedString_placeholder.h"

@implementation NSAttributedString

+allocWithZone:(NSZone *)zone {
   if(self==[NSAttributedString class])
    return NSAllocateObject([NSAttributedString_placeholder class],0,NULL);

   return NSAllocateObject(self,0,zone);
}

/* Minkar: an empty string, through initWithString: when the class has
   one; a subclass with only the primitive methods (string and
   attributesAtIndex:effectiveRange:, as TextMate's menu titles) is
   initialised as an object, as on the reference platform, where calling
   the abstract initWithString: raised. */
-init {
   if(class_getMethodImplementation(object_getClass(self),@selector(initWithString:))==
      class_getMethodImplementation([NSAttributedString class],@selector(initWithString:)))
    return [super init];
   return [self initWithString:@""];
}

-initWithString:(NSString *)string {
   NSInvalidAbstractInvocation();
   return nil;
}

-initWithString:(NSString *)string attributes:(NSDictionary *)attributes {
   NSInvalidAbstractInvocation();
   return nil;
}

-initWithAttributedString:(NSAttributedString *)other {
   NSInvalidAbstractInvocation();
   return nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
	NSUnimplementedMethod();
}

-initWithCoder:(NSCoder *)coder {
	NSUnimplementedMethod();
   return self;
}

-copy {
   return [self retain];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

-mutableCopy {
   return [[NSMutableAttributedString allocWithZone:NULL] initWithAttributedString:self];
}

-mutableCopyWithZone:(NSZone *)zone {
   return [[NSMutableAttributedString allocWithZone:zone] initWithAttributedString:self];
}

-(BOOL)isEqualToAttributedString:(NSAttributedString *)other {
	if ([self length] != [other length]) {
		return NO;
	}
	unsigned i = 0;
	NSString* string = [self string];
	NSString* otherString = [other string];
	if (![string isEqualToString:otherString]) {
		return NO;
	}
	unsigned length = [self length];
	NSRange range;
	while(i<length) {
		NSDictionary* attributes = [self attributesAtIndex: i effectiveRange: &range];
		NSRange otherRange;
		NSDictionary* otherAttributes = [other attributesAtIndex: i effectiveRange: &otherRange];
		if (![attributes isEqualToDictionary:otherAttributes]) {
			return NO;
		}
		i = MIN(NSMaxRange(range), NSMaxRange(otherRange));
	}
	return YES;
}

- (NSUInteger)hash
{
	return [[self string] hash];
}

- (BOOL)isEqual:(id)other
{
	if (self == other) {
		return YES;
	}
	if ([other isKindOfClass: [NSAttributedString class]]) {
		return [self isEqualToAttributedString: other];
	}
	return NO;
}

-(NSUInteger)length {
   return [[self string] length];
}

-(NSString *)string {
   NSInvalidAbstractInvocation();
   return nil;
}

-(NSDictionary *)attributesAtIndex:(NSUInteger)location effectiveRange:(NSRange *)range {
   NSInvalidAbstractInvocation();
   return nil;
}

/* The attributes at the location, and the longest range around it inside
   inRange where they are the same. */
-(NSDictionary *)attributesAtIndex:(NSUInteger)location
   longestEffectiveRange:(NSRange *)range inRange:(NSRange)inRange {
   NSRange       effective;
   NSDictionary *attributes=[self attributesAtIndex:location effectiveRange:&effective];

   if(range!=NULL){
    NSUInteger start=effective.location,end=NSMaxRange(effective);

    while(start>inRange.location){
     NSRange before;
     if(![[self attributesAtIndex:start-1 effectiveRange:&before] isEqualToDictionary:attributes])
      break;
     start=before.location;
    }
    while(end<NSMaxRange(inRange)){
     NSRange after;
     if(![[self attributesAtIndex:end effectiveRange:&after] isEqualToDictionary:attributes])
      break;
     end=NSMaxRange(after);
    }
    start=MAX(start,inRange.location);
    end=MIN(end,NSMaxRange(inRange));
    *range=NSMakeRange(start,end-start);
   }
   return attributes;
}

-attribute:(NSString *)name atIndex:(NSUInteger)location effectiveRange:(NSRange *)range {
   return [[self attributesAtIndex:location effectiveRange:range] objectForKey:name];
}

/* The attribute at the location, and the longest range around it inside
   inRange where it is the same. */
-attribute:(NSString *)name atIndex:(NSUInteger)location longestEffectiveRange:(NSRange *)range inRange:(NSRange)inRange {
   NSRange effective;
   id      value=[self attribute:name atIndex:location effectiveRange:&effective];

   if(range!=NULL){
    NSUInteger start=effective.location,end=NSMaxRange(effective);

    while(start>inRange.location){
     NSRange before;
     id      other=[self attribute:name atIndex:start-1 effectiveRange:&before];
     if(!(other==value || [other isEqual:value]))
      break;
     start=before.location;
    }
    while(end<NSMaxRange(inRange)){
     NSRange after;
     id      other=[self attribute:name atIndex:end effectiveRange:&after];
     if(!(other==value || [other isEqual:value]))
      break;
     end=NSMaxRange(after);
    }
    start=MAX(start,inRange.location);
    end=MIN(end,NSMaxRange(inRange));
    *range=NSMakeRange(start,end-start);
   }
   return value;
}

/* The runs of attributes in the range, front to back (or back to front),
   each the longest unless that is not required. */
-(void)enumerateAttributesInRange:(NSRange)enumerationRange options:(NSAttributedStringEnumerationOptions)opts
   usingBlock:(void (^)(NSDictionary *attrs, NSRange range, BOOL *stop))block {
   NSUInteger limit=MIN(NSMaxRange(enumerationRange),[self length]);
   BOOL       longest=!(opts&NSAttributedStringEnumerationLongestEffectiveRangeNotRequired);
   BOOL       stop=NO;

   if(enumerationRange.location>=limit)
    return;
   enumerationRange=NSMakeRange(enumerationRange.location,limit-enumerationRange.location);
   if(opts&NSAttributedStringEnumerationReverse){
    NSUInteger end=limit;
    while(end>enumerationRange.location && !stop){
     NSRange       range;
     NSDictionary *attributes=longest?[self attributesAtIndex:end-1 longestEffectiveRange:&range inRange:enumerationRange]:
                                      [self attributesAtIndex:end-1 effectiveRange:&range];
     range=NSIntersectionRange(range,NSMakeRange(enumerationRange.location,end-enumerationRange.location));
     block(attributes,range,&stop);
     end=range.location;
    }
    return;
   }
   NSUInteger location=enumerationRange.location;
   while(location<limit && !stop){
    NSRange       range;
    NSDictionary *attributes=longest?[self attributesAtIndex:location longestEffectiveRange:&range inRange:enumerationRange]:
                                     [self attributesAtIndex:location effectiveRange:&range];
    range=NSIntersectionRange(range,NSMakeRange(location,limit-location));
    block(attributes,range,&stop);
    location=NSMaxRange(range);
   }
}

-(void)enumerateAttribute:(NSString *)attrName inRange:(NSRange)enumerationRange options:(NSAttributedStringEnumerationOptions)opts
   usingBlock:(void (^)(id value, NSRange range, BOOL *stop))block {
   NSUInteger limit=MIN(NSMaxRange(enumerationRange),[self length]);
   BOOL       longest=!(opts&NSAttributedStringEnumerationLongestEffectiveRangeNotRequired);
   BOOL       stop=NO;

   if(enumerationRange.location>=limit)
    return;
   enumerationRange=NSMakeRange(enumerationRange.location,limit-enumerationRange.location);
   if(opts&NSAttributedStringEnumerationReverse){
    NSUInteger end=limit;
    while(end>enumerationRange.location && !stop){
     NSRange range;
     id      value=longest?[self attribute:attrName atIndex:end-1 longestEffectiveRange:&range inRange:enumerationRange]:
                           [self attribute:attrName atIndex:end-1 effectiveRange:&range];
     range=NSIntersectionRange(range,NSMakeRange(enumerationRange.location,end-enumerationRange.location));
     block(value,range,&stop);
     end=range.location;
    }
    return;
   }
   NSUInteger location=enumerationRange.location;
   while(location<limit && !stop){
    NSRange range;
    id      value=longest?[self attribute:attrName atIndex:location longestEffectiveRange:&range inRange:enumerationRange]:
                          [self attribute:attrName atIndex:location effectiveRange:&range];
    range=NSIntersectionRange(range,NSMakeRange(location,limit-location));
    block(value,range,&stop);
    location=NSMaxRange(range);
   }
}

-(NSAttributedString *)attributedSubstringFromRange:(NSRange)range {
   NSMutableAttributedString *result=[[[NSMutableAttributedString allocWithZone:NULL] init] autorelease];
   NSUInteger  location=range.location;
   NSUInteger  limit=MIN(NSMaxRange(range), [self length]);

   while(location<limit){
    NSRange         effectiveRange,appendedRange;
    NSDictionary   *attributes=[self attributesAtIndex:location effectiveRange:&effectiveRange];

    if(effectiveRange.location<location){
     effectiveRange.length=NSMaxRange(effectiveRange)-location;
     effectiveRange.location=location;
    }
    if(NSMaxRange(effectiveRange)>limit)
     effectiveRange.length=limit-effectiveRange.location;

    appendedRange.location=[result length];
    appendedRange.length=effectiveRange.length;
    [[result mutableString] appendString:[[self string] substringWithRange:effectiveRange]];
    [result setAttributes:attributes range:appendedRange];

    location=NSMaxRange(effectiveRange);
   }
   return result;
}

- (NSString*)description
{
	if ([self length] > 0) {
		NSMutableString *result = [NSMutableString string];
		unsigned length = [self length];
		int i = 0;
		while(i<length) {
			NSRange range;
			NSDictionary* attributes = [self attributesAtIndex: i effectiveRange: &range];
			NSString *string = [[self string] substringWithRange:range];
			[result appendFormat:@"%@ {\n%@\n}\n", string, attributes];
			i = NSMaxRange(range);
		}
		return result;
	} else {
		return [NSString stringWithFormat: @"%@ {}", [self string]];
	}
}

@end
