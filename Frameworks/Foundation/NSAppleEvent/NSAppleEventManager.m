// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Objective-C handlers for Apple events: each (class, ID) pair installed
// with AE goes to a target and selector, given the event and its reply.

#import <Foundation/NSAppleEventManager.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNotificationCenter.h>
#import <Foundation/NSException.h>
#include <stdlib.h>
#include <objc/message.h>

const double NSAppleEventTimeOutDefault=-1.0;
const double NSAppleEventTimeOutNone=-2.0;
NSString *const NSAppleEventManagerWillProcessFirstEventNotification=@"NSAppleEventManagerWillProcessFirstEventNotification";

/* A suspended event: the event, its reply, and where the reply goes. */
struct __NSAppleEventManagerSuspension {
   NSAppleEventDescriptor *event;
   NSAppleEventDescriptor *reply;
   AppleEvent             *rawReply;
};

@interface NSAppleEventManager () {
   NSMutableDictionary    *_handlers;
   NSAppleEventDescriptor *_currentEvent;
   NSAppleEventDescriptor *_currentReply;
   BOOL                    _sawFirstEvent;
   BOOL                    _suspended;
}
@end

@interface NSAppleEventHandlerEntry : NSObject {
@public
   id  _target;
   SEL _selector;
}
@end
@implementation NSAppleEventHandlerEntry
@end

static OSErr dispatchToManager(const AppleEvent *event, AppleEvent *reply, SRefCon refcon) {
   return [(NSAppleEventManager *)refcon dispatchRawAppleEvent:event withRawReply:reply handlerRefCon:refcon];
}

static NSNumber *keyFor(AEEventClass eventClass, AEEventID eventID) {
   return [NSNumber numberWithUnsignedLongLong:((unsigned long long)eventClass<<32)|eventID];
}

@implementation NSAppleEventManager

+(NSAppleEventManager *)sharedAppleEventManager {
   static NSAppleEventManager *shared;
   @synchronized(self){
    if(shared==nil)
     shared=[[NSAppleEventManager alloc] init];
   }
   return shared;
}

-init {
   self=[super init];
   if(self!=nil)
    _handlers=[[NSMutableDictionary alloc] init];
   return self;
}

-(void)dealloc {
   [_handlers release];
   [super dealloc];
}

-(void)setEventHandler:(id)handler andSelector:(SEL)selector forEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID {
   NSAppleEventHandlerEntry *entry=[[[NSAppleEventHandlerEntry alloc] init] autorelease];
   entry->_target=handler;
   entry->_selector=selector;
   @synchronized(self){
    [_handlers setObject:entry forKey:keyFor(eventClass,eventID)];
   }
   AEInstallEventHandler(eventClass,eventID,dispatchToManager,(SRefCon)self,false);
}

-(void)removeEventHandlerForEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID {
   @synchronized(self){
    [_handlers removeObjectForKey:keyFor(eventClass,eventID)];
   }
   AERemoveEventHandler(eventClass,eventID,dispatchToManager,false);
}

-(NSAppleEventHandlerEntry *)_entryForClass:(AEEventClass)eventClass eventID:(AEEventID)eventID {
   const AEEventClass classes[4]={ eventClass, eventClass, typeWildCard, typeWildCard };
   const AEEventID    ids[4]={ eventID, typeWildCard, eventID, typeWildCard };
   @synchronized(self){
    for(int i=0;i<4;i++){
     NSAppleEventHandlerEntry *entry=[_handlers objectForKey:keyFor(classes[i],ids[i])];
     if(entry!=nil)
      return [[entry retain] autorelease];
    }
   }
   return nil;
}

-(OSErr)dispatchRawAppleEvent:(const AppleEvent *)rawEvent withRawReply:(AppleEvent *)rawReply handlerRefCon:(SRefCon)refcon {
   AEDesc                    eventCopy,replyCopy;
   NSAppleEventDescriptor   *event,*reply;
   NSAppleEventDescriptor   *savedEvent=_currentEvent,*savedReply=_currentReply;
   NSAppleEventHandlerEntry *entry;
   OSErr                     result=noErr;

   if(!_sawFirstEvent){
    _sawFirstEvent=YES;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSAppleEventManagerWillProcessFirstEventNotification object:self];
   }
   AEDuplicateDesc(rawEvent,&eventCopy);
   event=[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&eventCopy];
   entry=[self _entryForClass:[event eventClass] eventID:[event eventID]];
   if(entry==nil){
    [event release];
    return errAEEventNotHandled;
   }
   if(rawReply!=NULL)
    AEDuplicateDesc(rawReply,&replyCopy);
   else
    AEInitializeDesc(&replyCopy);
   reply=[[NSAppleEventDescriptor alloc] initWithAEDescNoCopy:&replyCopy];
   _currentEvent=event;
   _currentReply=reply;
   _suspended=NO;
   @try {
    ((void (*)(id,SEL,id,id))objc_msgSend)(entry->_target,entry->_selector,event,reply);
   }
   @catch(NSException *exception){
    result=errAEEventFailed;
   }
   /* The handler filled in the reply descriptor: hand it back. */
   if(rawReply!=NULL && !_suspended){
    AEDisposeDesc(rawReply);
    AEDuplicateDesc([reply aeDesc],rawReply);
   }
   _currentEvent=savedEvent;
   _currentReply=savedReply;
   [event release];
   [reply release];
   return result;
}

-(NSAppleEventDescriptor *)currentAppleEvent {
   return _currentEvent;
}

-(NSAppleEventDescriptor *)currentReplyAppleEvent {
   return _currentReply;
}

-(NSAppleEventManagerSuspensionID)suspendCurrentAppleEvent {
   struct __NSAppleEventManagerSuspension *suspension;
   if(_currentEvent==nil)
    return NULL;
   suspension=calloc(1,sizeof(*suspension));
   suspension->event=[_currentEvent retain];
   suspension->reply=[_currentReply retain];
   _suspended=YES;
   return suspension;
}

-(NSAppleEventDescriptor *)appleEventForSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID {
   return suspensionID->event;
}

-(NSAppleEventDescriptor *)replyAppleEventForSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID {
   return suspensionID->reply;
}

-(void)setCurrentAppleEventAndReplyEventWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID {
   /* Not retained: the suspension keeps them until it is resumed. */
   _currentEvent=suspensionID->event;
   _currentReply=suspensionID->reply;
}

/* Events are answered inside AESendMessage: a resumed event's reply has
   nowhere left to go. */
-(void)resumeWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID {
   struct __NSAppleEventManagerSuspension *suspension=(struct __NSAppleEventManagerSuspension *)suspensionID;
   if(_currentEvent==suspension->event){
    _currentEvent=nil;
    _currentReply=nil;
   }
   [suspension->event release];
   [suspension->reply release];
   free(suspension);
}

@end
