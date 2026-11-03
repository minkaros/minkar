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
 * WSDisplay: one display of the new server (WindowServer plan, section 4:
 * Displays/). Ported from the legacy server's IOFramebufferDisplay and
 * WSDisplay: the IOFramebuffer server connection, the current mode and the
 * EDID identity. Drawing comes with the compositor (WS2).
 */

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include "SkyLight.h"

@class WSConnection, WSWindow;

@interface WSDisplay : NSObject

/* Every framebuffer the server could open; the first is the main display. */
+ (NSArray<WSDisplay *> *)availableDisplays;

@property (readonly) SLSDisplayID displayID;
@property (readonly) uint32_t flags;
@property (readonly) uint32_t width;		/* pixels of the current mode */
@property (readonly) uint32_t height;
@property (readonly) uint32_t vendor;
@property (readonly) uint32_t model;
@property (readonly) uint32_t serial;
/* What names the display across restarts, for its saved configuration:
 * the monitor's vendor, model and serial when it has an EDID, else where
 * its framebuffer is in the registry. */
@property (readonly) NSString *persistentKey;
@property (readonly) double widthMM;		/* from the EDID, else 72 dpi */
@property (readonly) double heightMM;
@property uint32_t unit;
/* The display this one mirrors, or 0; setting it sets kSLSDisplayMirrored. */
@property (nonatomic) SLSDisplayID mirrorOf;
@property double x;
@property double y;
/* The menu bar's height at the top of the display, in points. */
@property uint32_t menuBarHeight;

/* Maps the framebuffer's scanout memory (write-combined); NO when it
 * cannot. The mapping lasts until the next call or the display's end. */
- (BOOL)mapFramebuffer:(uint8_t **)base rowBytes:(uint32_t *)rowBytes;

/* Tells the framebuffer's driver what a frame changed, for devices that
 * show only what they are told (virtio-gpu). Nothing for the others. */
- (void)flushX:(uint32_t)x y:(uint32_t)y width:(uint32_t)width height:(uint32_t)height;

/*
 * Presenting through the GPU (G2): when the framebuffer names a companion
 * service (MinkarAcceleratorClass), its user client takes the shadow (page-
 * aligned memory, laid out as the framebuffer) into the GPU, and its DMA
 * engine copies ranges of it to the same place in the framebuffer. NO
 * when there is no such GPU or it refuses; the caller copies itself then.
 */
- (BOOL)setUpGPUPresentFrom:(void *)shadow bytes:(size_t)bytes;
- (void)tearDownGPUPresent;
- (BOOL)gpuPresentOffset:(size_t)offset bytes:(size_t)bytes;
/*
 * For a compositor that draws on the GPU (G4): count frames (1 or 2) of
 * bytes in VRAM, shared (their names, for Vulkan to import), mapped
 * for the CPU (for display streams). NO when there is no such GPU or it
 * refuses. A frame is presented by copying ranges of it into the
 * framebuffer (gpuPresentFrame:), or shown as it is from the next vertical
 * blank (flipToGPUFrame:, which returns once it is; -1 shows the
 * framebuffer again, as tearDownGPUPresent does).
 */
- (BOOL)setUpGPUFrames:(unsigned)count bytes:(size_t)bytes names:(uint64_t *)names
    cpu:(uint8_t **)cpu;
- (BOOL)gpuPresentFrame:(unsigned)index offset:(size_t)offset bytes:(size_t)bytes;
- (BOOL)flipToGPUFrame:(int)index;
/* The companion GPU's render node (/dev/dri/renderD<n>), or 0. */
@property (readonly) uint32_t gpuRenderMinor;
/* Its registry entry ID (IOSurfaces in its memory name it), or 0. */
@property (readonly) uint64_t gpuEntryID;

/* The display's compositor (WSCompositor), made at startup. */
@property (strong) id compositor;

/* The connection that captured the display, and its shielding window. */
@property (weak) WSConnection *capturedBy;
@property (strong) WSWindow *shield;

/* The reply to display_info. */
- (void)getInfo:(SLSDisplayInfo *)info;

/* The framebuffer's modes, read at startup. */
@property (readonly) uint32_t modeCount;
- (const SLSDisplayMode *)modeAtIndex:(uint32_t)index;
- (const SLSDisplayMode *)modeWithID:(uint32_t)modeID;
- (const SLSDisplayMode *)currentMode;

/* Switches the framebuffer to a mode of modeWithID:. */
- (int)setModeID:(uint32_t)modeID;

/*
 * The transfer (gamma) table: kSLSGammaTableCapacity entries per channel,
 * red, green, blue. The compositor applies it as it presents (with the
 * fade, WSDisplayColorTable); drivers with gamma hardware may later.
 */
- (int)setTransferTable:(const float *)table count:(uint32_t)count;
- (void)getTransferTable:(float *)table;
- (int)setTransferFormula:(const SLSGammaFormula *)formula;
@property (readonly) SLSGammaFormula transferFormula;
- (void)restoreTransfer;

@end
