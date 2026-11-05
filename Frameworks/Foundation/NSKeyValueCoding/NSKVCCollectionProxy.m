/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSKVCCollectionProxy.h"
#import <Foundation/NSString.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSMutableSet.h>
#import <Foundation/NSOrderedSet.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSKeyValueObserving.h>
#import <string.h>

/* Every message goes to a mutable copy of the property's value; after one
 * that changes it, the copy becomes the value (through the setter, so
 * observers are told). The object's own mutators are used when it has
 * them: add<Key>Object:, remove<Key>Object: for a set, and
 * insertObject:in<Key>AtIndex:, removeObjectFrom<Key>AtIndex: for an
 * ordered set. */

@implementation NSKVCCollectionProxy

-(id)initWithKey:(NSString *)key forProxyObject:(id)object ordered:(BOOL)ordered {
   _object=[object retain];
   _key=[key copy];
   _ordered=ordered;
   return self;
}

-(void)dealloc {
   [_object release];
   [_key release];
   [super dealloc];
}

-(NSString *)_capitalizedKey {
   if([_key length]==0)
    return _key;
   return [[[_key substringToIndex:1] uppercaseString] stringByAppendingString:[_key substringFromIndex:1]];
}

// The property's value, as a new mutable collection of the right kind.
-(id)_mutableValue {
   id value=[_object valueForKey:_key];

   if(_ordered){
    if([value isKindOfClass:[NSOrderedSet class]])
     return [[value mutableCopy] autorelease];
    if([value isKindOfClass:[NSArray class]])
     return [NSMutableOrderedSet orderedSetWithArray:value];
    if([value isKindOfClass:[NSSet class]])
     return [NSMutableOrderedSet orderedSetWithSet:value];
    return [NSMutableOrderedSet orderedSet];
   }
   if([value isKindOfClass:[NSSet class]])
    return [[value mutableCopy] autorelease];
   if(value!=nil)
    return [NSMutableSet setWithArray:[value isKindOfClass:[NSOrderedSet class]]?[value array]:value];
   return [NSMutableSet set];
}

-(NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
   static NSMutableSet *set;
   static NSMutableOrderedSet *orderedSet;

   if(set==nil){
    set=[[NSMutableSet alloc] init];
    orderedSet=[[NSMutableOrderedSet alloc] init];
   }
   return [(_ordered?(id)orderedSet:(id)set) methodSignatureForSelector:selector];
}

static BOOL isMutator(SEL selector) {
   const char *name=sel_getName(selector);
   const char *prefixes[]={ "add", "remove", "insert", "replace", "set", "exchange", "move", "sort", "union", "minus", "intersect", "filter", NULL };
   int i;

   for(i=0;prefixes[i]!=NULL;i++)
    if(strncmp(name,prefixes[i],strlen(prefixes[i]))==0)
     return YES;
   return NO;
}

// The object's own mutator for this message, if it has one.
-(BOOL)_forwardToAccessor:(NSInvocation *)invocation {
   SEL       selector=[invocation selector];
   NSString *ukey=[self _capitalizedKey];
   id        argument=nil;
   SEL       accessor=NULL;

   if(_ordered){
    if(selector==@selector(insertObject:atIndex:)){
     NSUInteger index;

     accessor=NSSelectorFromString([NSString stringWithFormat:@"insertObject:in%@AtIndex:",ukey]);
     if(![_object respondsToSelector:accessor])
      return NO;
     [invocation getArgument:&argument atIndex:2];
     [invocation getArgument:&index atIndex:3];
     ((void (*)(id,SEL,id,NSUInteger))[_object methodForSelector:accessor])(_object,accessor,argument,index);
     return YES;
    }
    if(selector==@selector(removeObjectAtIndex:)){
     NSUInteger index;

     accessor=NSSelectorFromString([NSString stringWithFormat:@"removeObjectFrom%@AtIndex:",ukey]);
     if(![_object respondsToSelector:accessor])
      return NO;
     [invocation getArgument:&index atIndex:2];
     ((void (*)(id,SEL,NSUInteger))[_object methodForSelector:accessor])(_object,accessor,index);
     return YES;
    }
    return NO;
   }

   if(selector==@selector(addObject:))
    accessor=NSSelectorFromString([NSString stringWithFormat:@"add%@Object:",ukey]);
   else if(selector==@selector(removeObject:))
    accessor=NSSelectorFromString([NSString stringWithFormat:@"remove%@Object:",ukey]);
   else if(selector==@selector(unionSet:))
    accessor=NSSelectorFromString([NSString stringWithFormat:@"add%@:",ukey]);
   else if(selector==@selector(minusSet:))
    accessor=NSSelectorFromString([NSString stringWithFormat:@"remove%@:",ukey]);

   if(accessor==NULL || ![_object respondsToSelector:accessor])
    return NO;
   [invocation getArgument:&argument atIndex:2];
   ((void (*)(id,SEL,id))[_object methodForSelector:accessor])(_object,accessor,argument);
   return YES;
}

-(void)forwardInvocation:(NSInvocation *)invocation {
   id target;

   if(isMutator([invocation selector]) && [self _forwardToAccessor:invocation])
    return;

   target=[self _mutableValue];
   [invocation invokeWithTarget:target];
   if(isMutator([invocation selector]))
    [_object setValue:target forKey:_key];
}

// Asked of the proxy itself, as of the collection it stands for.
-(BOOL)isKindOfClass:(Class)class {
   return [[self _mutableValue] isKindOfClass:class];
}

-(NSString *)description {
   return [[self _mutableValue] description];
}

-(NSUInteger)hash {
   return [[self _mutableValue] hash];
}

-(BOOL)isEqual:(id)other {
   return [[self _mutableValue] isEqual:other];
}

@end
