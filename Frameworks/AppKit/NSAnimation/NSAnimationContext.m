/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import "NSAnimationContext.h"
#import <Foundation/NSThread.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <dispatch/dispatch.h>

static NSString *const kContextStackKey = @"NSAnimationContextStack";

@implementation NSAnimationContext

static NSMutableArray *contextStack(void) {
   NSMutableDictionary *thread=[[NSThread currentThread] threadDictionary];
   NSMutableArray      *stack=[thread objectForKey:kContextStackKey];

   if(stack==nil){
    NSAnimationContext *base=[[[NSAnimationContext alloc] init] autorelease];
    stack=[NSMutableArray arrayWithObject:base];
    [thread setObject:stack forKey:kContextStackKey];
   }
   return stack;
}

-init {
   self=[super init];
   if(self!=nil)
    _duration=0.25;
   return self;
}

-(void)dealloc {
   [_timingFunction release];
   [_completionHandler release];
   [super dealloc];
}

-(id)copyWithZone:(NSZone *)zone {
   return [self retain];
}

+(NSAnimationContext *)currentContext {
   return [contextStack() lastObject];
}

/* A group starts with the enclosing one's settings, the completion
   handler aside. */
+(void)beginGrouping {
   NSAnimationContext *outer=[self currentContext];
   NSAnimationContext *group=[[[NSAnimationContext alloc] init] autorelease];

   group->_duration=outer->_duration;
   group->_timingFunction=[outer->_timingFunction retain];
   group->_allowsImplicitAnimation=outer->_allowsImplicitAnimation;
   [contextStack() addObject:group];
}

+(void)endGrouping {
   NSMutableArray     *stack=contextStack();
   NSAnimationContext *group;
   void              (^completion)(void);

   if([stack count]<=1)
    return;
   group=[[stack lastObject] retain];
   [stack removeLastObject];
   completion=[group->_completionHandler copy];
   if(completion!=nil){
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(MAX(group->_duration,0)*NSEC_PER_SEC)),dispatch_get_main_queue(),^{
       completion();
       [completion release];
    });
   }
   [group release];
}

+(void)runAnimationGroup:(void (^)(NSAnimationContext *))changes completionHandler:(void (^)(void))completionHandler {
   [self beginGrouping];
   [[self currentContext] setCompletionHandler:completionHandler];
   if(changes!=nil)
    changes([self currentContext]);
   [self endGrouping];
}

+(void)runAnimationGroup:(void (^)(NSAnimationContext *))changes {
   [self runAnimationGroup:changes completionHandler:nil];
}

-(NSTimeInterval)duration {
   return _duration;
}

-(void)setDuration:(NSTimeInterval)duration {
   _duration=duration;
}

-(CAMediaTimingFunction *)timingFunction {
   return _timingFunction;
}

-(void)setTimingFunction:(CAMediaTimingFunction *)function {
   [function retain];
   [_timingFunction release];
   _timingFunction=function;
}

-(void (^)(void))completionHandler {
   return _completionHandler;
}

-(void)setCompletionHandler:(void (^)(void))handler {
   handler=[handler copy];
   [_completionHandler release];
   _completionHandler=handler;
}

-(BOOL)allowsImplicitAnimation {
   return _allowsImplicitAnimation;
}

-(void)setAllowsImplicitAnimation:(BOOL)value {
   _allowsImplicitAnimation=value;
}

@end
