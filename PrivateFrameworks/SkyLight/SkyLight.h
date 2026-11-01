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
 * SkyLight: the window server's client side (WindowServer plan, section
 * 4). It wraps the MIG interface (IPC/WindowServer.defs); CoreGraphics
 * and AppKit use it. Functions are added with each milestone.
 */

#ifndef _SKYLIGHT_H
#define _SKYLIGHT_H

#include <stdint.h>
#include <mach/mach.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The window server's Mach service. The legacy server keeps
 * "org.minkar.WindowServer" until WS4, when this one replaces it. */
#define kSLSServiceName		"org.minkar.WindowServer.next"

/* The interface version both sides are built for. */
#define kSLSInterfaceVersion	1

typedef int32_t SLSConnectionID;

/*
 * The process's connection to the window server, opened on first use;
 * 0 when there is no server.
 */
SLSConnectionID SLSMainConnectionID(void);

/* The server's port (MACH_PORT_NULL when there is none), for the other
 * client functions. */
mach_port_t SLSServerPort(void);

#ifdef __cplusplus
}
#endif

#endif /* _SKYLIGHT_H */
