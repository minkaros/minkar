/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What NSConnection, NSPortCoder and NSDistantObject share inside
 * Foundation (Distributed Objects over Mach ports).
 *
 * A message is one NSData (the coded payload) followed by the ports it
 * carries. The payload starts with its kind and sequence number:
 *   request  target, flags, selector, types, then the arguments
 *   reply    status, then the return value and what out arguments point
 *            to, or an exception's name and reason
 *   root     (no body): the reply carries the root object
 *   signature target, selector: the reply carries the method's types
 * Objects travel by copy (values, collections, NSCoding objects asked for
 * bycopy) or by reference: an id that the sender keeps for one of its
 * objects, which the receiver turns into an NSDistantObject, or, when it
 * comes back, into the object itself.
 */

#import <Foundation/NSConnection.h>
#import <Foundation/NSDistantObject.h>
#import <Foundation/NSPortCoder.h>

enum {
   NSDOMessageRequest=1,
   NSDOMessageReply=2,
   NSDOMessageRoot=3,
   NSDOMessageSignature=4,
};

enum {
   NSDORequestOneway=1,
};

enum {
   NSDOReplyValue=0,
   NSDOReplyException=1,
   NSDOReplyNoTarget=2,
};

@interface NSPortCoder (NSDistributedObjects)
-initForSendingWithConnection:(NSConnection *)connection;
-initForReceivingWithConnection:(NSConnection *)connection components:(NSArray *)components;
-(NSArray *)encodedComponents;
-(void)setBycopy:(BOOL)bycopy byref:(BOOL)byref;
-(void)encodeUInt32:(uint32_t)value;
-(uint32_t)decodeUInt32;
-(void)encodeCString:(const char *)string;
-(NSString *)decodeString;
-(BOOL)isAtEnd;
@end

@interface NSDistantObject (NSDistributedObjects)
-initWithWireID:(uint32_t)wireID connection:(NSConnection *)connection;
-(uint32_t)_wireID;
-(id)_localObject;
-(BOOL)_isLocal;
-(Protocol *)_protocol;
@end

@interface NSConnection (NSDistributedObjects)
-(uint32_t)_exportLocalObject:(id)object;
-(id)_localObjectForWireID:(uint32_t)wireID;
-(NSDistantObject *)_proxyForWireID:(uint32_t)wireID;
-(void)_forwardInvocation:(NSInvocation *)invocation forProxy:(NSDistantObject *)proxy;
-(NSMethodSignature *)_methodSignatureForSelector:(SEL)selector proxy:(NSDistantObject *)proxy;
@end
