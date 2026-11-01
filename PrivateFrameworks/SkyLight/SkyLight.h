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

/* ---- Displays (the CGDirectDisplay API) ------------------------------- */

/* Results, with the values of the public CGError. */
enum {
	kSLSErrorSuccess	= 0,
	kSLSErrorFailure	= 1000,
	kSLSErrorIllegalArgument = 1001,
	kSLSErrorRangeCheck	= 1007,
};

typedef uint32_t SLSDisplayID;

/* SLSDisplayInfo.flags */
enum {
	kSLSDisplayActive	= 1 << 0,
	kSLSDisplayOnline	= 1 << 1,
	kSLSDisplaySleeping	= 1 << 2,
	kSLSDisplayMirrored	= 1 << 3,
	kSLSDisplayPrimary	= 1 << 4,
	kSLSDisplayMain		= 1 << 5,
	kSLSDisplayBuiltin	= 1 << 6,
	kSLSDisplayStereo	= 1 << 7,
	kSLSDisplayHWMirror	= 1 << 8,
};

/* Everything the server knows about one display, in one reply. Bounds are
 * in global coordinates (points), the main display at the origin. */
typedef struct SLSDisplayInfo {
	SLSDisplayID	display;
	uint32_t	flags;
	double		x, y, width, height;
	uint32_t	vendor, model, serial, unit;
	double		rotation;		/* degrees, clockwise */
	double		widthMM, heightMM;	/* physical size */
	SLSDisplayID	mirrorOf;		/* 0 when not a mirror */
	SLSDisplayID	primary;		/* of its mirror set */
	uint32_t	openGLMask;
	uint32_t	reserved;
} SLSDisplayInfo;

/* The size the MIG interface carries (IPC/WindowServer.defs). */
#define kSLSDisplayInfoWords	24
#ifndef __cplusplus
_Static_assert(sizeof(SLSDisplayInfo) == kSLSDisplayInfoWords * 4, "SLSDisplayInfo size");
#endif

SLSDisplayID SLSMainDisplayID(void);
int SLSGetOnlineDisplayList(uint32_t max, SLSDisplayID *displays, uint32_t *count);
int SLSGetActiveDisplayList(uint32_t max, SLSDisplayID *displays, uint32_t *count);
int SLSGetDisplaysWithPoint(double x, double y, uint32_t max, SLSDisplayID *displays,
    uint32_t *count);
int SLSGetDisplaysWithRect(double x, double y, double width, double height,
    uint32_t max, SLSDisplayID *displays, uint32_t *count);
int SLSGetDisplayInfo(SLSDisplayID display, SLSDisplayInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* _SKYLIGHT_H */
