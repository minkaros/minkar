/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Key-value coding on sets, as on the reference: valueForKey: is the set
 * of each member's value (no element for a nil one, unlike an array's
 * NSNull), an operator (@count, @sum, @avg, @max, @min, the unions) is
 * the members' as an array's would be; setValue:forKey: sets each member's.
 */

#import <Foundation/Foundation.h>

@implementation NSSet (NSKeyValueCoding)

-(id)valueForKey:(NSString *)key {
   if([key hasPrefix:@"@"]){
    id result=[[self allObjects] valueForKey:key];

    /* A union of a set is a set. */
    if([result isKindOfClass:[NSArray class]] && ([key isEqualToString:@"@distinctUnionOfObjects"] || [key hasPrefix:@"@distinctUnionOf"]))
     return [NSSet setWithArray:result];
    return result;
   }

   NSMutableSet *result=[NSMutableSet setWithCapacity:[self count]];
   for(id member in self){
    id value=[member valueForKey:key];

    if(value!=nil)
     [result addObject:value];
   }
   return result;
}

-(id)valueForKeyPath:(NSString *)keyPath {
   if([keyPath hasPrefix:@"@"])
    return [[self allObjects] valueForKeyPath:keyPath];

   NSRange dot=[keyPath rangeOfString:@"."];
   if(dot.location==NSNotFound)
    return [self valueForKey:keyPath];
   return [[self valueForKey:[keyPath substringToIndex:dot.location]] valueForKeyPath:[keyPath substringFromIndex:NSMaxRange(dot)]];
}

-(void)setValue:(id)value forKey:(NSString *)key {
   for(id member in self)
    [member setValue:value forKey:key];
}

@end
