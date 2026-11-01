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
@property (readonly) double widthMM;		/* from the EDID, else 72 dpi */
@property (readonly) double heightMM;
@property uint32_t unit;
/* The display this one mirrors, or 0; setting it sets kSLSDisplayMirrored. */
@property (nonatomic) SLSDisplayID mirrorOf;
@property double x;
@property double y;

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
 * red, green, blue. SHORTCUT(WS1): kept here only; the compositor applies
 * it from WS2, and drivers with gamma hardware come later.
 */
- (int)setTransferTable:(const float *)table count:(uint32_t)count;
- (void)getTransferTable:(float *)table;
- (int)setTransferFormula:(const SLSGammaFormula *)formula;
@property (readonly) SLSGammaFormula transferFormula;
- (void)restoreTransfer;

@end
