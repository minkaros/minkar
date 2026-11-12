/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSTimer_concrete.h>
#import <Foundation/NSString.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSRunLoop-InputSource.h>

@implementation NSTimer_concrete

/* Minkar: a timer in a run loop is a CFRunLoopTimer, which holds it until
   it is invalidated. It fires at the timer's fire date, and is then set to
   the next one (or goes, if the timer is no longer valid). */
static const void *retainTimer(const void *info) {
   return [(id)info retain];
}

static void releaseTimer(const void *info) {
   [(id)info release];
}

static void timerFired(CFRunLoopTimerRef cfTimer,void *info) {
   NSTimer_concrete  *timer=[(NSTimer_concrete *)info retain];
   NSAutoreleasePool *pool=[NSAutoreleasePool new];

   if([timer isValid])
    [timer fire];
   if([timer isValid])
    CFRunLoopTimerSetNextFireDate(cfTimer,[[timer fireDate] timeIntervalSinceReferenceDate]);
   else
    [timer _unschedule];
   [pool release];
   [timer release];
}

-(void)_scheduleInRunLoop:(CFRunLoopRef)runLoop mode:(NSString *)mode {
   if(!_isValid)
    return;
   if(_cfTimer==NULL){
    CFRunLoopTimerContext context={0,self,retainTimer,releaseTimer,NULL};

    // A long interval: CF's timer repeats (a one-shot one invalidates
    // itself), and is set to each fire date the timer has.
    _cfTimer=CFRunLoopTimerCreate(NULL,[_fireDate timeIntervalSinceReferenceDate],1.0e9,0,0,timerFired,&context);
   }
   CFRunLoopAddTimer(runLoop,_cfTimer,NSRunLoopCFMode(mode));
}

-(void)_unschedule {
   CFRunLoopTimerRef cfTimer=_cfTimer;

   if(cfTimer==NULL)
    return;
   _cfTimer=NULL;
   CFRunLoopTimerInvalidate(cfTimer);
   CFRelease(cfTimer);
}

-initWithFireDate:(NSDate *)date interval:(NSTimeInterval)interval repeats:(BOOL)repeats {
   _timeInterval=interval;
   _fireDate=[date copy];

   _isValid=YES;
   _repeats=repeats;

   return self;
}

-initWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats {

   _timeInterval=interval;
   _fireDate=[[[NSDate date] addTimeInterval:_timeInterval] retain];

   _isValid=YES;
   _repeats=repeats;

   return self;
}


-(void)dealloc {
   [_fireDate release];
   NSDeallocateObject(self);
   return;
   [super dealloc];
}


-(void)fire {
   if(!_repeats)
    [self invalidate];
   else if(_isValid) {
    NSDate *lastFire=_fireDate;
    // We add to the current time to get the next time, if we add to the fire date
    // and the timer is a long running task, the timer is always behind and always
    // catching up
    _fireDate=[[[NSDate date] addTimeInterval:_timeInterval] retain];
    [lastFire release];
   }
}


-(NSDate *)fireDate {
   return _fireDate;
}

-(NSTimeInterval)timeInterval {
   return _timeInterval;
}


-userInfo {
   return nil;
}

-(void)setFireDate:(NSDate *)date {
   date=[date copy];
   [_fireDate release];
   _fireDate=date;
   if(_cfTimer!=NULL)
    CFRunLoopTimerSetNextFireDate(_cfTimer,[_fireDate timeIntervalSinceReferenceDate]);
}

-(BOOL)isValid {
   return _isValid;
}

@end

