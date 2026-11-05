/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSCompoundPredicate.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRaise.h>

@implementation NSCompoundPredicate

-initWithType:(NSCompoundPredicateType)type subpredicates:(NSArray *)predicates {
   if(type==NSNotPredicateType && [predicates count]!=1)
    [NSException raise:NSInvalidArgumentException format:@"A NOT predicate takes one subpredicate, not %lu",(unsigned long)[predicates count]];
   _type=type;
   _predicates=[predicates copy];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   return [super initWithCoder:coder];
}

-(void)dealloc {
   [_predicates release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

+(NSCompoundPredicate *)notPredicateWithSubpredicate:(NSPredicate *)predicate {
   return [[[self alloc] initWithType:NSNotPredicateType subpredicates:[NSArray arrayWithObject:predicate]] autorelease];
}

+(NSCompoundPredicate *)andPredicateWithSubpredicates:(NSArray *)predicates {
   return [[[self alloc] initWithType:NSAndPredicateType subpredicates:predicates] autorelease];
}

+(NSCompoundPredicate *)orPredicateWithSubpredicates:(NSArray *)predicates {
   return [[[self alloc] initWithType:NSOrPredicateType subpredicates:predicates] autorelease];
}

// A subpredicate of another kind of compound is parenthesized.
-(NSString *)_formatOf:(NSPredicate *)check {
   NSString *format=[check predicateFormat];

   if([check isKindOfClass:[NSCompoundPredicate class]] &&
      ([(NSCompoundPredicate *)check compoundPredicateType]!=_type || _type==NSNotPredicateType) &&
      [[(NSCompoundPredicate *)check subpredicates] count]>1)
    format=[NSString stringWithFormat:@"(%@)",format];
   return format;
}

-(NSString *)predicateFormat {
   NSMutableArray *args=[NSMutableArray array];

   for(NSPredicate *check in _predicates)
    [args addObject:[self _formatOf:check]];

   switch(_type){
    case NSNotPredicateType:
     return [NSString stringWithFormat:@"NOT %@",[args objectAtIndex:0]];

    case NSAndPredicateType:
     if([args count]==0)
      return @"TRUEPREDICATE";
     return [args componentsJoinedByString:@" AND "];

    case NSOrPredicateType:
     if([args count]==0)
      return @"FALSEPREDICATE";
     return [args componentsJoinedByString:@" OR "];
   }
   return nil;
}

-(NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables {
   NSMutableArray *predicates=[NSMutableArray arrayWithCapacity:[_predicates count]];

   for(NSPredicate *check in _predicates)
    [predicates addObject:[check predicateWithSubstitutionVariables:variables]];

   return [[[NSCompoundPredicate alloc] initWithType:_type subpredicates:predicates] autorelease];
}

-(NSCompoundPredicateType)compoundPredicateType {
   return _type;
}

-(NSArray *)subpredicates {
   return _predicates;
}

-(BOOL)evaluateWithObject:object substitutionVariables:(NSDictionary *)bindings {
   switch(_type){
    case NSNotPredicateType:
     return ![[_predicates objectAtIndex:0] evaluateWithObject:object substitutionVariables:bindings];

    case NSAndPredicateType:
     for(NSPredicate *predicate in _predicates)
      if(![predicate evaluateWithObject:object substitutionVariables:bindings])
       return NO;
     return YES;

    case NSOrPredicateType:
     for(NSPredicate *predicate in _predicates)
      if([predicate evaluateWithObject:object substitutionVariables:bindings])
       return YES;
     return NO;
   }
   return NO;
}

@end
