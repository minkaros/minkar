/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSRunLoop.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSMapTable.h>
#import <Foundation/NSNotificationQueue-Private.h>
#import <Foundation/NSThread-Private.h>
#import <Foundation/NSDelayedPerform.h>
#import <Foundation/NSOrderedPerform.h>
#import <Foundation/NSRunLoop-InputSource.h>
#import <Foundation/NSPlatform.h>
#import <Foundation/NSSelectInputSource.h>
#import <Foundation/NSSocket.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSRaise.h>
#import <CoreFoundation/CFFileDescriptor.h>

/* Minkar: NSRunLoop is built on CFRunLoop, as on the reference platform.
   Each NSRunLoop is its thread's CFRunLoop: running it runs CF's (so CF's
   sources, timers and observers, and dispatch's main queue on the main
   thread, run under AppKit too); its timers are CFRunLoopTimers; a select
   input source (a descriptor: AppKit's event connection, file handles,
   sockets) is watched by a CFFileDescriptor; ordered performs and the
   notification queue's ASAP and idle notifications are served by a source
   and an observer of each mode they are wanted in. */

NSString * const NSDefaultRunLoopMode=@"kCFRunLoopDefaultMode";
NSString * const NSRunLoopCommonModes=@"kCFRunLoopCommonModes";

@interface NSRunLoop(NSRunLoopCF)
-(void)_bindToCFRunLoop:(CFRunLoopRef)runLoop;
@end

@interface NSTimer(NSRunLoopCF)
-(void)_scheduleInRunLoop:(CFRunLoopRef)runLoop mode:(NSString *)mode;
@end

/* A select input source's descriptor, watched for the events it asks for
   (asked again before each wait: a source changes its mask). */
@interface NSRunLoopDescriptor : NSObject {
@public
   NSSelectInputSource *_source;
   CFFileDescriptorRef  _descriptor;
   CFRunLoopSourceRef   _runLoopSource;
   NSMutableArray      *_modes;
}
@end

@implementation NSRunLoopDescriptor

static void descriptorCallBack(CFFileDescriptorRef descriptor,CFOptionFlags flags,void *info) {
   NSRunLoopDescriptor *self=info;
   NSSelectInputSource *source=[[self->_source retain] autorelease];
   NSUInteger events=0;

   if(flags&kCFFileDescriptorReadCallBack)
    events|=NSSelectReadEvent;
   if(flags&kCFFileDescriptorWriteCallBack)
    events|=NSSelectWriteEvent;
   if(events && [source isValid])
    [source processImmediateEvents:events];
   [self arm];
}

-initWithSource:(NSSelectInputSource *)source {
   CFFileDescriptorContext context={0,self,NULL,NULL,NULL};

   _source=[source retain];
   _modes=[NSMutableArray new];
   _descriptor=CFFileDescriptorCreate(NULL,[[source socket] fileDescriptor],false,descriptorCallBack,&context);
   if(_descriptor!=NULL)
    _runLoopSource=CFFileDescriptorCreateRunLoopSource(NULL,_descriptor,0);
   if(_runLoopSource==NULL){
    [self release];
    return nil;
   }
   [self arm];
   return self;
}

-(void)dealloc {
   if(_runLoopSource!=NULL){
    CFRunLoopSourceInvalidate(_runLoopSource);
    CFRelease(_runLoopSource);
   }
   if(_descriptor!=NULL){
    CFFileDescriptorInvalidate(_descriptor);
    CFRelease(_descriptor);
   }
   [_source release];
   [_modes release];
   [super dealloc];
}

/* The callbacks for the events the source asks for now (CF's fire once). */
-(void)arm {
   NSUInteger   mask=[_source selectEventMask];
   CFOptionFlags enable=0,disable=0;

   if(mask&NSSelectReadEvent) enable|=kCFFileDescriptorReadCallBack; else disable|=kCFFileDescriptorReadCallBack;
   if(mask&NSSelectWriteEvent) enable|=kCFFileDescriptorWriteCallBack; else disable|=kCFFileDescriptorWriteCallBack;
   if(disable)
    CFFileDescriptorDisableCallBacks(_descriptor,disable);
   if(enable)
    CFFileDescriptorEnableCallBacks(_descriptor,enable);
}

@end

@implementation NSRunLoop

/* Each one is bound to its thread's CFRunLoop when it is first given out:
   the main thread's may be made on another thread. */
+(NSRunLoop *)currentRunLoop {
   NSRunLoop *runLoop=NSThreadSharedInstance(@"NSRunLoop");

   [runLoop _bindToCFRunLoop:[NSThread isMainThread]?CFRunLoopGetMain():CFRunLoopGetCurrent()];
   return runLoop;
}

+(NSRunLoop *)mainRunLoop {
	NSRunLoop *runLoop=[[NSThread mainThread] sharedObjectForClassName:@"NSRunLoop"];

   [runLoop _bindToCFRunLoop:CFRunLoopGetMain()];
   return runLoop;
}

static void performSourcePerform(void *info);
static void beforeWaiting(CFRunLoopObserverRef observer,CFRunLoopActivity activity,void *info);

-init {
   CFRunLoopSourceContext sourceContext={0};
   CFRunLoopObserverContext observerContext={0};

   _orderedPerforms=[NSMutableArray new];
   _timers=[NSMutableArray new];
   _descriptors=NSCreateMapTableWithZone(NSNonRetainedObjectMapKeyCallBacks,NSObjectMapValueCallBacks,0,[self zone]);
   _servedModes=[NSMutableSet new];

   sourceContext.info=self;
   sourceContext.perform=performSourcePerform;
   _performSource=CFRunLoopSourceCreate(NULL,0,&sourceContext);
   observerContext.info=self;
   _observer=CFRunLoopObserverCreate(NULL,kCFRunLoopBeforeWaiting|kCFRunLoopExit,true,0,beforeWaiting,&observerContext);
   return self;
}

-(void)_bindToCFRunLoop:(CFRunLoopRef)runLoop {
   NSInputSource *parentDeath;

   @synchronized(self) {
    if(_cfRunLoop!=NULL)
     return;
    _cfRunLoop=runLoop;
   }
   // AppKit's modal and event tracking modes are common modes, as on the
   // reference platform: what is added to the common modes runs in them.
   if(runLoop==CFRunLoopGetMain()){
    CFRunLoopAddCommonMode(runLoop,CFSTR("NSModalPanelRunLoopMode"));
    CFRunLoopAddCommonMode(runLoop,CFSTR("NSEventTrackingRunLoopMode"));
   }
   [self _serveMode:NSRunLoopCommonModes];

   if((parentDeath=[[NSPlatform currentPlatform] parentDeathInputSource])!=nil)
    [self addInputSource:parentDeath forMode:NSDefaultRunLoopMode];
}

-(void)dealloc {
   CFRunLoopSourceInvalidate(_performSource);
   CFRelease(_performSource);
   CFRunLoopObserverInvalidate(_observer);
   CFRelease(_observer);
   NSFreeMapTable(_descriptors);
   [_servedModes release];
   [_timers release];
	[_orderedPerforms release];
	[super dealloc];
}

/* Minkar: the CFRunLoop of the thread this run loop belongs to. */
- (CFRunLoopRef)getCFRunLoop {
   return (CFRunLoopRef)_cfRunLoop;
}

/* The perform source and the observer, in mode (once; from any thread:
   performs come from other threads). */
-(void)_serveMode:(NSString *)mode {
   @synchronized(_servedModes) {
    if([_servedModes containsObject:mode])
     return;
    [_servedModes addObject:mode];
   }
   CFRunLoopAddSource([self getCFRunLoop],_performSource,(CFStringRef)mode);
   CFRunLoopAddObserver([self getCFRunLoop],_observer,(CFStringRef)mode);
}

/* The modes a perform asked for, the common modes' name as the modes it
   stands for (the default one, and on the main thread AppKit's modal and
   event tracking modes). */
-(NSArray *)_resolveModes:(NSArray *)modes {
   NSMutableArray *result=[NSMutableArray array];

   for(NSString *mode in modes){
    if([mode isEqualToString:NSRunLoopCommonModes]){
     [result addObject:NSDefaultRunLoopMode];
     if([self getCFRunLoop]==CFRunLoopGetMain()){
      [result addObject:@"NSModalPanelRunLoopMode"];
      [result addObject:@"NSEventTrackingRunLoopMode"];
     }
    }
    else if(![result containsObject:mode])
     [result addObject:mode];
   }
   return result;
}

-(NSString *)currentMode {
   CFStringRef mode=CFRunLoopCopyCurrentMode([self getCFRunLoop]);

   return [(NSString *)mode autorelease];
}

/* The ordered performs that can run in mode, in their order. */
-(BOOL)_firePerformsInMode:(NSString *)mode {
   BOOL      didPerform=NO;
   NSArray  *performs;

   @synchronized(_orderedPerforms) {
    performs=[_orderedPerforms copy];
   }
   for(NSOrderedPerform *check in performs){
    BOOL pending;

    @synchronized(_orderedPerforms) {
     pending=[_orderedPerforms indexOfObjectIdenticalTo:check]!=NSNotFound;
     if(pending && [[check modes] containsObject:mode])
      [_orderedPerforms removeObjectIdenticalTo:check];
     else
      pending=NO;
    }
    // The lock is not held while the perform runs.
    if(pending && [check fireInMode:mode])
     didPerform=YES;
   }
   [performs release];
   return didPerform;
}

-(BOOL)_hasPerformsInMode:(NSString *)mode {
   @synchronized(_orderedPerforms) {
    for(NSOrderedPerform *check in _orderedPerforms)
     if([[check modes] containsObject:mode])
      return YES;
   }
   return NO;
}

static void performSourcePerform(void *info) {
   NSRunLoop *self=info;
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSString *mode=[self currentMode];

   if(mode!=nil)
    [self _firePerformsInMode:mode];
   [pool release];
}

/* Before the run loop waits in a mode: the notification queue's ASAP
   notifications, then its idle ones, and ordered performs of this mode
   (they may have come for it while another one ran); select sources
   asked again for the events they want. */
static void beforeWaiting(CFRunLoopObserverRef observer,CFRunLoopActivity activity,void *info) {
   NSRunLoop *self=info;
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSString *mode=[self currentMode];

   if(activity==kCFRunLoopBeforeWaiting && mode!=nil){
    NSNotificationQueue *queue=[NSNotificationQueue defaultQueue];

    [queue asapProcessMode:mode];
    if([queue hasIdleNotificationsInMode:mode]){
     [queue idleProcessMode:mode];
     CFRunLoopWakeUp([self getCFRunLoop]);
    }
    if([self _hasPerformsInMode:mode]){
     CFRunLoopSourceSignal(self->_performSource);
     CFRunLoopWakeUp([self getCFRunLoop]);
    }
   }
   {
    NSMapEnumerator state=NSEnumerateMapTable(self->_descriptors);
    NSSelectInputSource *source;
    NSRunLoopDescriptor *descriptor;
    NSMutableArray *invalid=nil;

    while(NSNextMapEnumeratorPair(&state,(void **)&source,(void **)&descriptor)){
     if([source isValid])
      [descriptor arm];
     else {
      if(invalid==nil)
       invalid=[NSMutableArray array];
      [invalid addObject:source];
     }
    }
    NSEndMapTableEnumeration(&state);
    // A source made invalid leaves every mode.
    for(NSSelectInputSource *check in invalid){
     NSRunLoopDescriptor *gone=NSMapGet(self->_descriptors,check);

     for(NSString *each in [[gone->_modes copy] autorelease])
      [self removeInputSource:check forMode:each];
    }
   }
   [pool release];
}

-(NSDate *)limitDateForMode:(NSString *)mode {
   CFRunLoopRef  runLoop=[self getCFRunLoop];
   CFAbsoluteTime next;

   [self _serveMode:mode];
   if(CFRunLoopRunInMode((CFStringRef)mode,0,true)==kCFRunLoopRunFinished)
    return nil;
   next=CFRunLoopGetNextTimerFireDate(runLoop,(CFStringRef)mode);
   if(next==0)
    return [NSDate distantFuture];
   return [NSDate dateWithTimeIntervalSinceReferenceDate:next];
}

-(void)acceptInputForMode:(NSString *)mode beforeDate:(NSDate *)date {
   [self _serveMode:mode];
   CFRunLoopRunInMode((CFStringRef)mode,MAX([date timeIntervalSinceNow],0),true);
}

/* One pass: until something was handled, the date or a stop. NO when
   the mode has nothing to wait for (or is the common modes' name). */
-(BOOL)runMode:(NSString *)mode beforeDate:(NSDate *)date {
   NSAutoreleasePool *pool;
   SInt32             result;

   if(mode==nil || [mode isEqualToString:NSRunLoopCommonModes])
    return NO;
   pool=[NSAutoreleasePool new];
   [self _serveMode:mode];
   result=CFRunLoopRunInMode((CFStringRef)mode,MAX([date timeIntervalSinceNow],0),true);
   [pool release];
   return result!=kCFRunLoopRunFinished;
}

-(void)runUntilDate:(NSDate *)date {
   while([self runMode:NSDefaultRunLoopMode beforeDate:date])
    if([date timeIntervalSinceNow]<=0)
     break;
}

-(void)run {
// Calling -run w/o a pool in place is valid, which is why we need one here
// for the NSDate. Could get rid of the pool if the date was not autoreleased.
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSDate            *future=[NSDate distantFuture];

   while([self runMode:NSDefaultRunLoopMode beforeDate:future])
    ;

   [pool release];
}

-(void)addPort:(NSPort *)port forMode:(NSString *)mode {
   NSUnimplementedMethod();
}

-(void)removePort:(NSPort *)port forMode:(NSString *)mode {
   NSUnimplementedMethod();
}

-(void)addInputSource:(NSInputSource *)source forMode:(NSString *)mode {
   NSRunLoopDescriptor *descriptor;

   if(![source isKindOfClass:[NSSelectInputSource class]]){
    NSLog(@"NSRunLoop: %@ is not a descriptor's input source; it is not watched",source);
    return;
   }
   descriptor=NSMapGet(_descriptors,source);
   if(descriptor==nil){
    descriptor=[[NSRunLoopDescriptor alloc] initWithSource:(NSSelectInputSource *)source];
    if(descriptor==nil)
     return;
    NSMapInsert(_descriptors,source,descriptor);
    [descriptor release];
   }
   if(![descriptor->_modes containsObject:mode]){
    [descriptor->_modes addObject:mode];
    CFRunLoopAddSource([self getCFRunLoop],descriptor->_runLoopSource,(CFStringRef)mode);
    [self _serveMode:mode];
   }
}

-(void)removeInputSource:(NSInputSource *)source forMode:(NSString *)mode {
   NSRunLoopDescriptor *descriptor=NSMapGet(_descriptors,source);

   if(descriptor==nil || ![descriptor->_modes containsObject:mode])
    return;
   CFRunLoopRemoveSource([self getCFRunLoop],descriptor->_runLoopSource,(CFStringRef)mode);
   [descriptor->_modes removeObject:mode];
   if([descriptor->_modes count]==0)
    NSMapRemove(_descriptors,source);
}

-(void)addTimer:(NSTimer *)timer forMode:(NSString *)mode {
   NSInteger count=[_timers count];

   // The timers kept to find delayed performs again: invalid ones go.
   while(--count>=0)
    if(![[_timers objectAtIndex:count] isValid])
     [_timers removeObjectAtIndex:count];
   if([_timers indexOfObjectIdenticalTo:timer]==NSNotFound)
    [_timers addObject:timer];
   [timer _scheduleInRunLoop:[self getCFRunLoop] mode:mode];
   [self _serveMode:mode];
}

-(void)invalidateTimerWithDelayedPerform:(NSDelayedPerform *)delayed {
   NSArray *timers=[[_timers copy] autorelease];

   for(NSTimer *timer in timers){
    NSDelayedPerform *check=[timer isValid]?[timer userInfo]:nil;

    if([check isKindOfClass:[NSDelayedPerform class]] && [check isEqualToPerform:delayed]){
     [timer invalidate];
     [_timers removeObjectIdenticalTo:timer];
    }
   }
}

-(void)performSelector:(SEL)selector target:target argument:argument order:(NSUInteger)order modes:(NSArray *)modes {
   NSArray          *resolved=[self _resolveModes:modes];
   NSOrderedPerform *perform=[NSOrderedPerform orderedPerformWithSelector:selector target:target argument:argument order:order modes:resolved];
	@synchronized(_orderedPerforms)
	{
		NSInteger count=[_orderedPerforms count];

		while(--count>=0){
			NSOrderedPerform *check=[_orderedPerforms objectAtIndex:count];
			NSUInteger          checkOrder=[check order];

			if(checkOrder<=order)
				break;
		}
		[_orderedPerforms insertObject:perform atIndex:count+1];
	}
   for(NSString *mode in resolved)
    [self _serveMode:mode];
   CFRunLoopSourceSignal(_performSource);
   CFRunLoopWakeUp([self getCFRunLoop]);
}

-(void)_runBlock:(void (^)(void))block {
   block();
}

-(void)performInModes:(NSArray *)modes block:(void (^)(void))block {
   id copy=[[block copy] autorelease];

   [self performSelector:@selector(_runBlock:) target:self argument:copy order:0 modes:modes];
}

-(void)performBlock:(void (^)(void))block {
   [self performInModes:[NSArray arrayWithObject:NSRunLoopCommonModes] block:block];
}

-(void)cancelPerformSelector:(SEL)selector target:target argument:argument {
	@synchronized(_orderedPerforms)
	{
		NSInteger count=[_orderedPerforms count];

		while(--count>=0){
			NSOrderedPerform *check=[_orderedPerforms objectAtIndex:count];

			if([check selector]==selector && [check target]==target && [check argument]==argument)
				[_orderedPerforms removeObjectAtIndex:count];
		}
	}
}

-(void)cancelPerformSelectorsWithTarget:target {
	@synchronized(_orderedPerforms)
	{
		NSInteger count=[_orderedPerforms count];

		while(--count>=0){
			NSOrderedPerform *check=[_orderedPerforms objectAtIndex:count];

			if([check target]==target)
				[_orderedPerforms removeObjectAtIndex:count];
		}
	}
}

@end

@implementation NSObject(NSDelayedPerform)

+(void)cancelPreviousPerformRequestsWithTarget:target
       selector:(SEL)selector object:argument {
   NSDelayedPerform *delayed=[NSDelayedPerform delayedPerformWithObject:target selector:selector argument:argument];

   [[NSRunLoop currentRunLoop] invalidateTimerWithDelayedPerform:delayed];
}

+(void)cancelPreviousPerformRequestsWithTarget:target {
   NSDelayedPerform *delayed=[NSDelayedPerform delayedPerformWithObject:target selector:NULL argument:nil];

   [[NSRunLoop currentRunLoop] invalidateTimerWithDelayedPerform:delayed];
}

+(void)_delayedPerform:(NSTimer *)timer {
   NSDelayedPerform *delayed=[timer userInfo];

   [delayed perform];
}

+(void)object:object performSelector:(SEL)selector withObject:argument afterDelay:(NSTimeInterval)delay inModes:(NSArray *)modes {
   NSDelayedPerform *delayed=[NSDelayedPerform delayedPerformWithObject:object selector:selector argument:argument];
   NSTimer          *timer=[NSTimer timerWithTimeInterval:delay target:[NSObject class] selector:@selector(_delayedPerform:) userInfo:delayed repeats:NO];
   NSInteger         i,count=[modes count];

   for(i=0;i<count;i++)
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:[modes objectAtIndex:i]];
}

-(void)performSelector:(SEL)selector withObject:object afterDelay:(NSTimeInterval)delay {
   [[self class] object:self performSelector:selector withObject:object afterDelay:delay inModes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
}

-(void)performSelector:(SEL)selector withObject:object afterDelay:(NSTimeInterval)delay inModes:(NSArray *)modes {
   [[self class] object:self performSelector:selector withObject:object afterDelay:delay inModes:modes];
}

@end

