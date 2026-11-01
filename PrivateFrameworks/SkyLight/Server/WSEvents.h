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
 * WSEvents: where input goes (WindowServer plan, section 4, Input/).
 * Mouse events go to the window under the pointer, or to the window that
 * got the mouse down until every button is up; a mouse down makes its
 * window key; keys go to the key window. Each connection has a bounded
 * queue, mouse moves coalesced, and its client is told when the queue
 * stops being empty; the server never blocks on a client. Held keys
 * repeat. Everything here runs on the IPC queue.
 */

#import "WSServer.h"

/* An event read from the HID system, to pass the taps and route. */
void WSRouteEvent(SLSEvent *event);
/* The same for an event entering at a tap location (posted). */
void WSRouteEventAt(SLSEvent *event, uint32_t location);

/* Event taps (WSTaps.m): NO when a filtering tap dropped the event; it
 * may have changed it. */
BOOL WSTapsFilter(SLSEvent *event, uint32_t location);
@class WSConnection;
void WSTapsConnectionDied(WSConnection *connection);
/* A client went away: if it was the front app, the one before it is. */
void WSEventsConnectionDied(WSConnection *connection);
/* The front app's connection, or nil. */
WSConnection *WSFrontConnection(void);
/* A window left the screen or went away: it stops being key (its app's
 * front window is key instead) and holding the pointer. */
@class WSWindow;
void WSEventsWindowGone(WSWindow *window);
/* Nanoseconds since boot (WSInput.m). */
uint64_t WSNanoseconds(void);
