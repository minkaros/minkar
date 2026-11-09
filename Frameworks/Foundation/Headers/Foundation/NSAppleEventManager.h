// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <CoreServices/CoreServices.h>

@class NSAppleEventDescriptor;

NS_ASSUME_NONNULL_BEGIN

typedef const struct __NSAppleEventManagerSuspension *NSAppleEventManagerSuspensionID;

FOUNDATION_EXPORT const double NSAppleEventTimeOutDefault;
FOUNDATION_EXPORT const double NSAppleEventTimeOutNone;
FOUNDATION_EXPORT NSString *const NSAppleEventManagerWillProcessFirstEventNotification;

@interface NSAppleEventManager : NSObject

+ (NSAppleEventManager *)sharedAppleEventManager;

/* The handler gets handleEvent:(NSAppleEventDescriptor *)event
   withReplyEvent:(NSAppleEventDescriptor *)reply. */
- (void)setEventHandler:(id)handler andSelector:(SEL)handleEventSelector forEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID;
- (void)removeEventHandlerForEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID;
- (OSErr)dispatchRawAppleEvent:(const AppleEvent *)theAppleEvent withRawReply:(AppleEvent *)theReply handlerRefCon:(SRefCon)handlerRefCon;

@property (nullable, readonly, retain) NSAppleEventDescriptor *currentAppleEvent;
@property (nullable, readonly, retain) NSAppleEventDescriptor *currentReplyAppleEvent;

- (nullable NSAppleEventManagerSuspensionID)suspendCurrentAppleEvent NS_RETURNS_INNER_POINTER;
- (NSAppleEventDescriptor *)appleEventForSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID;
- (NSAppleEventDescriptor *)replyAppleEventForSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID;
- (void)setCurrentAppleEventAndReplyEventWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID;
- (void)resumeWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID;

@end

NS_ASSUME_NONNULL_END
