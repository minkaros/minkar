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

#import "NSDispatchMainQueueSource.h"
#import <Foundation/NSSocket.h>
#include <mach/mach.h>
#include <sys/event.h>
#include <unistd.h>

extern mach_port_t _dispatch_get_main_queue_port_4CF(void);
extern void _dispatch_main_queue_callback_4CF(void *msg);

@implementation NSDispatchMainQueueSource

+ (NSDispatchMainQueueSource *)sharedSource {
    static NSDispatchMainQueueSource *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        shared = [[NSDispatchMainQueueSource alloc] init];
    });
    return shared;
}

- (id)init {
    mach_port_t port = _dispatch_get_main_queue_port_4CF();
    int kq = (port != MACH_PORT_NULL) ? kqueue() : -1;
    struct kevent change;

    if (kq < 0) {
        [self release];
        return nil;
    }
    // Without MACH_RCV_MSG the event only says that a message is waiting;
    // the message itself is received below.
    EV_SET(&change, port, EVFILT_MACHPORT, EV_ADD | EV_ENABLE, 0, 0, NULL);
    if (kevent(kq, &change, 1, NULL, 0, NULL) < 0) {
        close(kq);
        [self release];
        return nil;
    }
    _kqueue = kq;
    NSSocket *socket = [[[NSSocket alloc] initWithFileDescriptor:kq] autorelease];
    if ((self = [super initWithSocket:socket]) == nil)
        return nil;
    [self setSelectEventMask:NSSelectReadEvent];
    return self;
}

- (NSUInteger)processImmediateEvents:(NSUInteger)selectEvent {
    if ((selectEvent & NSSelectReadEvent) == 0)
        return 0;

    struct kevent event;
    struct timespec zero = { 0, 0 };
    mach_port_t port = _dispatch_get_main_queue_port_4CF();
    union {
        mach_msg_header_t header;
        uint8_t bytes[256];
    } msg;

    kevent(_kqueue, NULL, 0, &event, 1, &zero);
    // The wakeup messages (the port holds one at most): while one waits,
    // the event stays on.
    while (port != MACH_PORT_NULL) {
        msg.header.msgh_size = sizeof(msg);
        msg.header.msgh_local_port = port;
        if (mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
                     sizeof(msg), port, 0, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
            break;
        mach_msg_destroy(&msg.header);
    }
    _dispatch_main_queue_callback_4CF(NULL);
    return NSSelectReadEvent;
}

@end
