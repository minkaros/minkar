/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSCoder.h>
#import <dispatch/dispatch.h>
#import <xpc/xpc.h>
#import <bsm/audit.h>

@class NSError, NSSet, NSString;
@class NSXPCInterface, NSXPCListener, NSXPCListenerEndpoint;
@protocol NSXPCListenerDelegate;

NS_ASSUME_NONNULL_BEGIN

// Objects that hand out proxies for a remote object.
@protocol NSXPCProxyCreating

// A proxy for the remote object. Messages to it go out asynchronously.
- (id)remoteObjectProxy;

// The same, and errorHandler (instead of the reply block) when the
// message cannot be delivered or its reply does not come back.
- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;

@optional

// A proxy whose messages wait for their reply block before returning.
- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;

@end

typedef NS_OPTIONS(NSUInteger, NSXPCConnectionOptions) {
    // The service runs as a launchd daemon in the system domain.
    NSXPCConnectionPrivileged = (1 << 12UL)
};

// A two-way connection to another process: each side may export an
// object and call the other side's through a proxy.
@interface NSXPCConnection : NSObject <NSXPCProxyCreating>

// A connection to the XPC service of that name, inside the app's bundle.
- (instancetype)initWithServiceName:(NSString *)serviceName;
@property (nullable, readonly, copy) NSString *serviceName;

// A connection to a service that launchd advertises under name.
- (instancetype)initWithMachServiceName:(NSString *)name options:(NSXPCConnectionOptions)options;

// A connection to the listener that endpoint names.
- (instancetype)initWithListenerEndpoint:(NSXPCListenerEndpoint *)endpoint;
@property (readonly, retain) NSXPCListenerEndpoint *endpoint;

@property (nullable, retain) NSXPCInterface *exportedInterface;
@property (nullable, retain) id exportedObject;
@property (nullable, retain) NSXPCInterface *remoteObjectInterface;

@property (readonly, retain) id remoteObjectProxy;
- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;
- (id)synchronousRemoteObjectProxyWithErrorHandler:(void (^)(NSError *error))handler;

// Called when the other process exits or crashes; the connection may be
// used again. Called on another queue.
@property (nullable, copy) void (^interruptionHandler)(void);

// Called when the connection cannot be used any more.
@property (nullable, copy) void (^invalidationHandler)(void);

- (void)resume;
- (void)suspend;
- (void)invalidate;

@property (readonly) au_asid_t auditSessionIdentifier;
@property (readonly) pid_t processIdentifier;
@property (readonly) uid_t effectiveUserIdentifier;
@property (readonly) gid_t effectiveGroupIdentifier;

// The connection whose message is being handled on this thread, or nil.
+ (nullable NSXPCConnection *)currentConnection;

// block runs once every message sent before it has been sent.
- (void)scheduleSendBarrierBlock:(void (^)(void))block;

@end

// Listens for new connections: to a service of the app's bundle, to a
// launchd service, or anonymously (reached through its endpoint).
@interface NSXPCListener : NSObject

// The listener of an XPC service; -resume never returns.
+ (NSXPCListener *)serviceListener;

// A listener reached only through its endpoint.
+ (NSXPCListener *)anonymousListener;

// A listener for the launchd service of that name.
- (instancetype)initWithMachServiceName:(NSString *)name;

@property (nullable, weak) id <NSXPCListenerDelegate> delegate;
@property (readonly, retain) NSXPCListenerEndpoint *endpoint;

- (void)resume;
- (void)suspend;
- (void)invalidate;

@end

@protocol NSXPCListenerDelegate <NSObject>
@optional
// YES to accept the connection (after configuring and resuming it).
- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection;
@end

// What one side calls on the other: a protocol, and for each argument the
// classes it may hold.
@interface NSXPCInterface : NSObject

+ (NSXPCInterface *)interfaceWithProtocol:(Protocol *)protocol;

@property (assign) Protocol *protocol;

// The classes argument arg of sel (of its reply block, with ofReply) may
// hold. A collection argument names its collection and its elements.
- (void)setClasses:(NSSet *)classes forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (NSSet *)classesForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;

// That argument goes as a proxy to an object exported with ifc.
- (void)setInterface:(NSXPCInterface *)ifc forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (nullable NSXPCInterface *)interfaceForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;

// That argument is an XPC object of type type (an xpc_object_t argument).
- (void)setXPCType:(xpc_type_t)type forSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;
- (nullable xpc_type_t)XPCTypeForSelector:(SEL)sel argumentIndex:(NSUInteger)arg ofReply:(BOOL)ofReply;

@end

// Names a listener; may be sent over a connection and used by the other
// side to connect to it.
@interface NSXPCListenerEndpoint : NSObject <NSSecureCoding>
@end

// The coder of messages over a connection.
@interface NSXPCCoder : NSCoder

- (void)encodeXPCObject:(xpc_object_t)xpcObject forKey:(NSString *)key;
- (nullable xpc_object_t)decodeXPCObjectOfType:(xpc_type_t)type forKey:(NSString *)key;

@property (nullable, retain) id <NSObject> userInfo;
@property (nullable, readonly, strong) NSXPCConnection *connection;

@end

NS_ASSUME_NONNULL_END
