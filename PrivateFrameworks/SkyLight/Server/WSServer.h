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
 * What the new server's parts share: the displays, the connections and
 * the windows, all used on the IPC queue only.
 */

#import <Foundation/Foundation.h>
#include <bsm/libbsm.h>
#include "SkyLight.h"
#import "WSDisplay.h"

@class WSConnection, WSWindow, WSFrameItem;

/* The queue every request is served on; the model changes only there. */
extern dispatch_queue_t WSIPCQueue;

/* Displays (WSDisplayServices.m). The first is the main display. */
extern NSArray<WSDisplay *> *WSDisplays;
WSDisplay *WSDisplayWithID(uint32_t displayID);
/* A connection's process exited: release what it held on the displays. */
void WSDisplayServicesConnectionDied(WSConnection *connection);

/* Windows (WSWindowList.m). */
WSWindow *WSWindowCreate(WSConnection *owner, int32_t level, SLSRect frame);
void WSWindowRelease(WSWindow *window);
void WSWindowSetFrame(WSWindow *window, SLSRect frame);
int WSWindowOrder(WSWindow *window, int32_t place, SLSWindowID relativeTo);
/* Releases every window of the connection; returns how many. */
unsigned WSWindowsConnectionDied(WSConnection *connection);
/* The on-screen windows back to front, for a frame (IPC queue). */
NSArray<WSFrameItem *> *WSWindowSnapshot(void);
/* The cursor's position; YES when it shows (IPC queue). */
BOOL WSCursorSnapshot(SLSPoint *position);
/* Counts a surface commit in the statistics. */
void WSStatsCountCommit(void);
/* The window list: the on-screen window at a point (front most), or nil;
 * any window by its ID. */
WSWindow *WSWindowAtPoint(double x, double y);
WSWindow *WSWindowWithID(SLSWindowID windowID);
/* The cursor: where it is, and a move by input (kept on the displays). */
SLSPoint WSCursorPosition(void);
void WSCursorMoveTo(double x, double y);
/* Counts an input event in the statistics. */
void WSStatsCountEvent(void);
/* Queues an event for a connection (WSEvents.m). */
void WSEventsEnqueue(WSConnection *connection, const SLSEvent *event);
/* Whether a connection may control input: post, filter, listen to keys,
 * claim symbolic hotkeys (WSTaps.m). */
BOOL WSMayControlInput(WSConnection *connection);
/* Symbolic hotkeys (WSHotKeys.m): reads the table at start; for a key
 * down that is one, sends it to its owner and returns YES (the key and its
 * up are not delivered). keyTarget is the key window, for hotkeys owned by
 * the front app. */
void WSHotKeysStart(void);
BOOL WSHotKeysHandle(const SLSEvent *event, WSWindow *keyTarget);
/* Tells the HID system where a warp put the cursor (WSInput.m). */
void WSInputSetCursor(double x, double y);
/* A posted key event without characters gets the keymap's (WSInput.m). */
void WSInputTranslatePosted(SLSEvent *event);
