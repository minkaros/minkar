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
 * WSDisplayStream: a display's frames into IOSurfaces of a client
 * (CGDisplayStream, WindowServer plan WS5). The compositor offers each
 * frame it draws; the stream copies it, scaled to the output size, into a
 * surface whose use count is 0, increments that count and tells the
 * client. A frame it cannot take yet (too soon, or no surface free) stays
 * pending, and the compositor draws that area again at the next tick.
 */

#import <Foundation/Foundation.h>
#include <IOSurface/IOSurfaceRef.h>
#include <pixman.h>
#include "SkyLight.h"

@class WSConnection;

@interface WSDisplayStream : NSObject

/* nil unless every surface exists and is BGRA of the output size. */
- (instancetype)initWithDisplayID:(uint32_t)display surfaces:(const uint32_t *)surfaces
    count:(uint32_t)count options:(SLSDisplayStreamOptions)options notify:(mach_port_t)notify
    owner:(WSConnection *)owner;

@property (readonly) uint32_t streamID;
@property (readonly) uint32_t displayID;
@property (readonly, weak) WSConnection *owner;
@property (readonly) BOOL showsCursor;

/* Render queue: a frame drew damage (display pixels) into shadow. NO when
 * it is kept for later; pending then has what to draw again. */
- (BOOL)offerFrame:(pixman_image_t *)shadow damage:(pixman_region32_t *)damage
    pending:(pixman_region32_t *)pending;

/* Tells the client the stream stopped; no frame after this. */
- (void)stop;

@end
