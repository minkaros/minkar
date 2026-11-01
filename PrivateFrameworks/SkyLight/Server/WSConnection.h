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

/*
 * WSConnection: one client process. The client passes a port it holds the
 * receive right to; the server keeps a send right and asks for a dead-name
 * notification, which arrives when the process exits. Requests are matched
 * to their connection by the audit token's PID and PID version.
 */

#import "WSServer.h"

@interface WSConnection : NSObject

@property (readonly) SLSConnectionID connectionID;
@property (readonly) pid_t pid;
@property (readonly) uid_t uid;
@property (readonly) mach_port_t port;

/* State the connection holds, released when it dies. */
@property int32_t cursorHideCount;
@property BOOL cursorAssociated;
@property BOOL changedTransfers;
/* Events waiting for the client (SLSEvents), and how many were dropped
 * because the queue was full. */
@property (readonly) NSMutableData *eventQueue;
@property uint32_t droppedEvents;
/* It asked to hear about windows minimized and restored. */
@property BOOL windowNotifications;
/* Displays changed for this app only: display ID -> @[mode ID, x, y,
 * mirror of] as they were before. */
@property (readonly) NSMutableDictionary<NSNumber *, NSArray<NSNumber *> *> *appOnlyDisplays;

/* Starts watching for dead clients on the IPC queue. */
+ (void)startOnQueue:(dispatch_queue_t)queue;

/* A new connection for the token's process, or the one already open on
 * port. */
+ (WSConnection *)connectionForPort:(mach_port_t)port token:(audit_token_t)token;
/* The connection of the request's process, or nil. */
+ (WSConnection *)connectionForToken:(audit_token_t)token;
+ (NSArray<WSConnection *> *)allConnections;

@end
