#import <Foundation/NSScriptWhoseTests.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSPredicate.h>

/* Comparisons any object can be asked for: through -compare: when it has
 * one, and LIKE (wildcards * and ?) for strings, as predicates use it. */

@implementation NSObject (NSComparisonMethods)

static NSComparisonResult compareObjects(id self,id other) {
   if(other==nil || ![self respondsToSelector:@selector(compare:)])
    return (NSComparisonResult)NSNotFound;
   return [(NSString *)self compare:other];
}

-(BOOL)isEqualTo:other {
   return [self isEqual:other];
}

-(BOOL)isNotEqualTo:other {
   return ![self isEqual:other];
}

-(BOOL)isLessThanOrEqualTo:other {
   NSComparisonResult result=compareObjects(self,other);

   return result==NSOrderedAscending || result==NSOrderedSame;
}

-(BOOL)isLessThan:other {
   return compareObjects(self,other)==NSOrderedAscending;
}

-(BOOL)isGreaterThanOrEqualTo:other {
   NSComparisonResult result=compareObjects(self,other);

   return result==NSOrderedDescending || result==NSOrderedSame;
}

-(BOOL)isGreaterThan:other {
   return compareObjects(self,other)==NSOrderedDescending;
}

-(BOOL)doesContain:object {
   if([self isKindOfClass:[NSArray class]])
    return [(NSArray *)self containsObject:object];
   return NO;
}

-(BOOL)isLike:(NSString *)pattern {
   if(![self isKindOfClass:[NSString class]] || pattern==nil)
    return NO;
   return [[NSPredicate predicateWithFormat:@"SELF LIKE %@",pattern] evaluateWithObject:self];
}

-(BOOL)isCaseInsensitiveLike:(NSString *)pattern {
   if(![self isKindOfClass:[NSString class]] || pattern==nil)
    return NO;
   return [[NSPredicate predicateWithFormat:@"SELF LIKE[c] %@",pattern] evaluateWithObject:self];
}

@end
