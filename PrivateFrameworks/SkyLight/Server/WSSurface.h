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
 * WSSurface: a window's backing store (WindowServer plan, section 4,
 * Surfaces/). Two buffers of premultiplied BGRA pixels in memory the
 * server allocates and shares with the window's owner through a Mach
 * memory entry; never a named shared memory object. The client draws into
 * the back buffer and commits it; the compositor reads the front one
 * while holding the surface's lock, so a commit never swaps under it.
 * An IOSurface takes this place later (roadmap step 4).
 */

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <IOSurface/IOSurfaceRef.h>

@interface WSSurface : NSObject

- (instancetype)initWithWidth:(uint32_t)width height:(uint32_t)height;

@property (readonly) uint32_t surfaceID;
@property (readonly) uint32_t width;
@property (readonly) uint32_t height;
@property (readonly) uint32_t rowBytes;
@property (readonly) mach_port_t memoryEntry;	/* both buffers */
/* The buffer the client draws into; the other one is shown. */
@property (readonly) uint32_t backBuffer;
/* Whether anything was committed yet: until then nothing is shown. */
@property (readonly) BOOL committed;

/* Shows buffer from the next frame; it must be the back one. */
- (BOOL)commitBuffer:(uint32_t)buffer;

/* The front buffer's pixels, valid until -unlock. */
- (const uint8_t *)lockFront;
- (void)unlock;

/* Whether its pixels change without commits (an IOSurface the app draws
 * into): the compositor shows it again at every frame. */
@property (readonly) BOOL live;

@end

/*
 * A window that shows an IOSurface its app made (SLSWindowSetIOSurface):
 * the same interface as WSSurface, one buffer, the app's. It is live, as
 * a display's scanout is: what the app draws shows at the next frame.
 */
@interface WSIOSurfaceBacking : NSObject

/* nil unless the surface exists and is BGRA, 4 bytes per pixel, of
 * this size. */
- (instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID width:(uint32_t)width
    height:(uint32_t)height;

@property (readonly) uint32_t surfaceID;
@property (readonly) uint32_t width;
@property (readonly) uint32_t height;
@property (readonly) uint32_t rowBytes;
@property (readonly) uint32_t backBuffer;	/* always 0 */
@property (readonly) BOOL committed;		/* always YES */
@property (readonly) BOOL live;			/* always YES */

- (BOOL)commitBuffer:(uint32_t)buffer;
- (const uint8_t *)lockFront;
- (void)unlock;

@end
