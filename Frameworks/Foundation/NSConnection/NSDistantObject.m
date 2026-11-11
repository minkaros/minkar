/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSDistantObject: an object of another process (a remote proxy, by the
 * id its owner keeps for it) or, for the moment it is encoded, one of
 * ours sent by reference (a local proxy). A remote proxy forwards every
 * message to its connection, which sends it and waits for the reply; the
 * method's signature comes from the protocol set for the proxy, else from
 * the other process.
 */

#import <Foundation/NSValue.h>
#import <Foundation/NSDistantObject.h>
#import <Foundation/NSConnection.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import "NSConnectionInternal.h"
#include <objc/runtime.h>

@implementation NSDistantObject

-initWithLocal:local connection:(NSConnection *)connection {
   _connection=connection;
   _localOrRemote=local;
   _isLocal=YES;
   return self;
}

// A proxy for what another proxy stands for, on this connection.
-initWithTarget:remote connection:(NSConnection *)connection {
   uint32_t wireID=[remote respondsToSelector:@selector(unsignedIntValue)]?[remote unsignedIntValue]:0;

   return [self initWithWireID:wireID connection:connection];
}

-initWithWireID:(uint32_t)wireID connection:(NSConnection *)connection {
   _connection=connection;
   _wireID=wireID;
   _isLocal=NO;
   return self;
}

+(NSDistantObject *)proxyWithLocal:local connection:(NSConnection *)connection {
   return [[self alloc] initWithLocal:local connection:connection];
}

+(NSDistantObject *)proxyWithTarget:remote connection:(NSConnection *)connection {
   return [[self alloc] initWithTarget:remote connection:connection];
}

-(NSConnection *)connectionForProxy {
   return _connection;
}

-(void)setProtocolForProxy:(Protocol *)protocol {
   _protocol=protocol;
}

-(uint32_t)_wireID {
   return _wireID;
}

-(id)_localObject {
   return _localOrRemote;
}

-(BOOL)_isLocal {
   return _isLocal;
}

-(Protocol *)_protocol {
   return _protocol;
}

-(NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
   if(_isLocal)
    return [_localOrRemote methodSignatureForSelector:selector];
   return [_connection _methodSignatureForSelector:selector proxy:self];
}

-(void)forwardInvocation:(NSInvocation *)invocation {
   if(_isLocal){
    [invocation invokeWithTarget:_localOrRemote];
    return;
   }
   [_connection _forwardInvocation:invocation forProxy:self];
}

-(Class)classForCoder {
   return [NSDistantObject class];
}

-(Class)classForPortCoder {
   return [NSDistantObject class];
}

-replacementObjectForPortCoder:(NSPortCoder *)coder {
   return self;
}

-(NSString *)description {
   if(_isLocal)
    return [NSString stringWithFormat:@"<%s %p local %@>",class_getName(object_getClass(self)),self,_localOrRemote];
   return [NSString stringWithFormat:@"<%s %p id %u connection %p>",class_getName(object_getClass(self)),self,_wireID,_connection];
}

@end
