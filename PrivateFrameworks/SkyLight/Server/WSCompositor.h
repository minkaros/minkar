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
 * WSCompositor: one display's compositor (WindowServer plan, section 4,
 * Compositor/), with pixman as its backend. Damage collects the screen
 * areas that changed; a display link at the mode's refresh rate (GOP has
 * no vsync) turns it into a frame: a snapshot of the windows is taken on
 * the IPC queue, and only the damaged area is composited, back to front,
 * into a shadow buffer and copied to the framebuffer, cursor last. No
 * damage, no frame.
 */

#import "WSServer.h"

/* One window as a frame draws it: taken on the IPC queue. */
@interface WSFrameItem : NSObject
@property (strong) id surface;		/* WSSurface, or nil */
@property SLSRect frame;		/* global coordinates */
@property double alpha;
@property BOOL solid;			/* a shielding window: opaque black */
@property BOOL shadow;			/* drawn with a shadow */
@end

@interface WSCompositor : NSObject

/* nil when the display's framebuffer cannot be mapped. */
- (instancetype)initWithDisplay:(WSDisplay *)display;
- (void)start;

/* Any thread: an area of the desktop (global coordinates) to redraw. */
- (void)damageGlobalRect:(SLSRect)rect;
- (void)damageAll;
/* The display's mode or origin changed (IPC queue). */
- (void)displayChanged;

/* Every compositor's counts together. */
+ (void)getStats:(SLSServerStats *)stats;

@end

/* Damage on every display the rectangle touches. */
void WSDamageGlobalRect(SLSRect rect);
