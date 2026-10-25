/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * Display backend for IOKit framebuffers (IOFramebuffer subclasses such
 * as IOGOPFramebuffer). WindowServer opens the framebuffer's server
 * connection, maps its scanout memory and composites into a software
 * back buffer that is copied to the screen on each frame. A GPU backend
 * can later replace the copy with a page flip without touching callers.
 */

#import "WSDisplay.h"
#include <IOKit/IOKitLib.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOFramebufferDisplay : WSDisplay

/* One display for every IOFramebuffer that could be opened. */
+ (NSArray<IOFramebufferDisplay *> *)availableDisplays;

- (nullable instancetype)initWithService:(io_service_t)service NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/* Open the server connection and map the screen. Returns NO on failure. */
- (BOOL)open;

/* Make this the primary and main display. */
- (void)markAsMain;

@end

NS_ASSUME_NONNULL_END
