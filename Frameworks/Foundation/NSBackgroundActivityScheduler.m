/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/NSBackgroundActivityScheduler.h>
#import <Foundation/NSString.h>
#include <dispatch/dispatch.h>

@implementation NSBackgroundActivityScheduler

-init {
   return [self initWithIdentifier:@""];
}

-initWithIdentifier:(NSString *)identifier {
   _identifier=[identifier copy];
   _qualityOfService=NSQualityOfServiceBackground;
   return self;
}

-(void)dealloc {
   [self invalidate];
   [_identifier release];
   [super dealloc];
}

-(NSString *)identifier { return _identifier; }
-(NSQualityOfService)qualityOfService { return _qualityOfService; }
-(void)setQualityOfService:(NSQualityOfService)value { _qualityOfService=value; }
-(BOOL)repeats { return _repeats; }
-(void)setRepeats:(BOOL)flag { _repeats=flag; }
-(NSTimeInterval)interval { return _interval; }
-(void)setInterval:(NSTimeInterval)value { _interval=value; }
-(NSTimeInterval)tolerance { return _tolerance; }
-(void)setTolerance:(NSTimeInterval)value { _tolerance=value; }

-(BOOL)shouldDefer {
   return NO;
}

static long priorityOfService(NSQualityOfService qos){
   switch(qos){
    case NSQualityOfServiceUserInteractive:
    case NSQualityOfServiceUserInitiated: return DISPATCH_QUEUE_PRIORITY_HIGH;
    case NSQualityOfServiceUtility:       return DISPATCH_QUEUE_PRIORITY_LOW;
    case NSQualityOfServiceBackground:    return DISPATCH_QUEUE_PRIORITY_BACKGROUND;
    default:                              return DISPATCH_QUEUE_PRIORITY_DEFAULT;
   }
}

/* A dispatch timer on the queue of the quality of service: each firing
   runs the block once at a time; a deferral tries again after the
   tolerance (or a minute). */
-(void)scheduleWithBlock:(void (^)(NSBackgroundActivityCompletionHandler completionHandler))block {
   dispatch_queue_t  queue=dispatch_get_global_queue(priorityOfService(_qualityOfService),0);
   dispatch_source_t timer;
   void (^work)(NSBackgroundActivityCompletionHandler)=[[block copy] autorelease];
   uint64_t          period=(uint64_t)(((_interval>0)?_interval:0)*NSEC_PER_SEC);
   uint64_t          leeway=(uint64_t)(((_tolerance>0)?_tolerance:_interval*0.1)*NSEC_PER_SEC);

   [self invalidate];
   timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,queue);
   _timer=(id)timer;
   [work retain];
   dispatch_source_set_event_handler(timer,^{
      if(_running)
       return;
      _running=YES;
      work(^(NSBackgroundActivityResult result){
         _running=NO;
         if(result==NSBackgroundActivityResultDeferred)
          dispatch_source_set_timer(timer,dispatch_time(DISPATCH_TIME_NOW,(int64_t)((_tolerance>0?_tolerance:60)*NSEC_PER_SEC)),
             _repeats?period:DISPATCH_TIME_FOREVER,leeway);
         else if(!_repeats)
          dispatch_source_cancel(timer);
      });
   });
   dispatch_source_set_cancel_handler(timer,^{
      [work release];
   });
   dispatch_source_set_timer(timer,dispatch_time(DISPATCH_TIME_NOW,(int64_t)period),(_repeats && period>0)?period:DISPATCH_TIME_FOREVER,leeway);
   dispatch_resume(timer);
}

-(void)invalidate {
   if(_timer!=nil){
    dispatch_source_cancel((dispatch_source_t)_timer);
    dispatch_release((dispatch_source_t)_timer);
    _timer=nil;
   }
}

@end
