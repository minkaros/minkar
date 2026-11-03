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
 * A compositor's backend (GPU track G4, WindowServer plan WS6): what draws
 * a frame and puts it on the display. WSCompositor decides what a frame
 * shows (the windows, the damage, the cursor) and when; a backend draws it
 * back to front, clipped to the damage, and presents it. WSPixmanBackend
 * draws with pixman into a buffer in memory and presents it by the GPU's
 * DMA engine or the CPU; WSVulkanBackend draws with compute shaders on the
 * GPU into a frame in VRAM and presents it the same way. Everything here runs
 * on the compositor's render queue.
 */

#import <pixman.h>
#import "WSServer.h"

@class WSDisplay;
@class WSSurface;

@protocol WSCompositorBackend <NSObject>

/* For the logs: what draws and what presents. */
@property (readonly) NSString *backendDescription;

/* Buffers for the display's current mode (again after a mode change); NO
 * when it cannot draw on this display. */
- (BOOL)setUpForDisplay:(WSDisplay *)display width:(uint32_t)width height:(uint32_t)height;
/* Its frame's size, for clipping the damage after a mode change. */
@property (readonly) uint32_t frameWidth, frameHeight;

/* A frame: the damage cleared to black; what follows is clipped to it. */
- (void)beginFrame:(pixman_region32_t *)damage;
/* Opaque black (a shielding window). */
- (void)fillBlackX:(int)x y:(int)y width:(int)width height:(int)height;
/* A window's drop shadow, under a window at (x, y) of this size, faded with
 * the window's alpha. */
- (void)drawShadowX:(int)x y:(int)y width:(int)width height:(int)height alpha:(double)alpha;
/* A window's surface at (x, y), over what is drawn, with its alpha. */
- (void)drawSurface:(WSSurface *)surface x:(int)x y:(int)y alpha:(double)alpha;
/* The windows are drawn; the clip goes. */
- (void)endWindows;

/* The frame in memory, for display streams (BGRA, the frame's size). */
- (pixman_image_t *)frameImage;

/* The cursor over the frame (ARGB pixels, w x h at x, y), until
 * removeCursor: it is presented with the frame, not kept in it. */
- (void)drawCursor:(pixman_image_t *)cursor x:(int)x y:(int)y width:(int)w height:(int)h;
- (void)removeCursor;

/* The damaged area to the display, through its colour table (red, green
 * and blue, 256 entries each) when there is one; the pixels presented. */
- (uint64_t)present:(pixman_region32_t *)damage colors:(NSData *)colors;

@end

/* The damaged rows as at most kWSMaxRowBands bands (y1, y2 pairs), rows
 * that overlap or touch merged: what a backend presents by the GPU's DMA
 * engine, a band at a time. Returns how many. */
enum { kWSMaxRowBands = 64 };
int WSRowBands(const pixman_box32_t *boxes, int n, int32_t *bands);

/* pixman in memory; presented by the GPU's DMA engine when the display has
 * one, else by the CPU. */
@interface WSPixmanBackend : NSObject <WSCompositorBackend>
@end

/* Vulkan compute shaders into a frame in the GPU's memory, presented by
 * its DMA engine; only on a display whose GPU has a Vulkan driver. */
@interface WSVulkanBackend : NSObject <WSCompositorBackend>
@end
