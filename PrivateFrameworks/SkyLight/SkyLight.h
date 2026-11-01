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
#define kSLSInterfaceVersion	2

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
	kSLSErrorInvalidConnection = 1002,
	kSLSErrorCannotComplete	= 1004,
	kSLSErrorRangeCheck	= 1007,
	kSLSErrorNoneAvailable	= 1011,
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

/* ---- Display modes and configuration ---------------------------------- */

/* One mode of a display. Sizes in points are the pixel sizes until
 * scaled modes exist. */
typedef struct SLSDisplayMode {
	uint32_t	modeID;			/* the framebuffer's mode ID */
	uint32_t	flags;			/* the framebuffer's mode flags */
	uint32_t	width, height;		/* points */
	uint32_t	pixelWidth, pixelHeight;
	uint32_t	depth;			/* bits per pixel */
	uint32_t	reserved;
	double		refreshRate;		/* Hz; 0 when unknown */
} SLSDisplayMode;

#define kSLSDisplayModeWords	10
#define kSLSMaxDisplayModes	64
#ifndef __cplusplus
_Static_assert(sizeof(SLSDisplayMode) == kSLSDisplayModeWords * 4, "SLSDisplayMode size");
#endif

/* The modes of a display, at most max (all of them with modes NULL: then
 * only the count). */
int SLSCopyDisplayModes(SLSDisplayID display, uint32_t max, SLSDisplayMode *modes,
    uint32_t *count);
int SLSGetCurrentDisplayMode(SLSDisplayID display, SLSDisplayMode *mode);

/* A configuration: changes collected on the client and sent at once by
 * SLSCompleteDisplayConfiguration. The server checks every change before
 * it applies any. */
typedef struct SLSDisplayConfiguration *SLSDisplayConfigRef;

/* SLSCompleteDisplayConfiguration's option, as the public CGConfigureOption. */
enum {
	kSLSConfigureForAppOnly		= 0,
	kSLSConfigureForSession		= 1,
	kSLSConfigurePermanently	= 2,
};

/* One change, as the MIG interface carries it. */
typedef struct SLSDisplayConfigOp {
	uint32_t	kind;			/* kSLSConfigOp* */
	SLSDisplayID	display;
	uint32_t	modeID;			/* kSLSConfigOpMode */
	int32_t		x, y;			/* kSLSConfigOpOrigin */
	SLSDisplayID	mirrorOf;		/* kSLSConfigOpMirror; 0 to stop */
	uint32_t	reserved[2];
} SLSDisplayConfigOp;

enum {
	kSLSConfigOpMode	= 1,
	kSLSConfigOpOrigin	= 2,
	kSLSConfigOpMirror	= 3,
};

#define kSLSDisplayConfigOpWords	8
#define kSLSMaxDisplayConfigOps		32
#ifndef __cplusplus
_Static_assert(sizeof(SLSDisplayConfigOp) == kSLSDisplayConfigOpWords * 4,
    "SLSDisplayConfigOp size");
#endif

int SLSBeginDisplayConfiguration(SLSDisplayConfigRef *config);
int SLSConfigureDisplayWithDisplayMode(SLSDisplayConfigRef config, SLSDisplayID display,
    uint32_t modeID);
int SLSConfigureDisplayOrigin(SLSDisplayConfigRef config, SLSDisplayID display,
    int32_t x, int32_t y);
/* Sends the changes and frees the configuration, whatever the result. */
/* display shows what primary shows; primary 0 stops mirroring. A display
 * mirrors neither itself nor a display that is a mirror. */
int SLSConfigureDisplayMirrorOfDisplay(SLSDisplayConfigRef config, SLSDisplayID display,
    SLSDisplayID primary);
int SLSCompleteDisplayConfiguration(SLSDisplayConfigRef config, uint32_t option);
int SLSCancelDisplayConfiguration(SLSDisplayConfigRef config);

/* ---- Gamma: display transfer tables ------------------------------------ */

/* Entries of each channel's table the server keeps. */
#define kSLSGammaTableCapacity	256

/* value = min + (max - min) * input ^ gamma, per channel. */
typedef struct SLSGammaFormula {
	float	redMin, redMax, redGamma;
	float	greenMin, greenMax, greenGamma;
	float	blueMin, blueMax, blueGamma;
} SLSGammaFormula;

#define kSLSGammaFormulaWords	9
#ifndef __cplusplus
_Static_assert(sizeof(SLSGammaFormula) == kSLSGammaFormulaWords * 4, "SLSGammaFormula size");
#endif

int SLSGetGammaTableCapacity(SLSDisplayID display, uint32_t *capacity);
/* Tables of count entries (2 to 256) each, values 0 to 1; resampled to the
 * capacity. */
int SLSSetDisplayTransferByTable(SLSDisplayID display, uint32_t count, const float *red,
    const float *green, const float *blue);
/* The table in use, at most capacity entries per channel. */
int SLSGetDisplayTransferByTable(SLSDisplayID display, uint32_t capacity, float *red,
    float *green, float *blue, uint32_t *count);
int SLSSetDisplayTransferByFormula(SLSDisplayID display, const SLSGammaFormula *formula);
/* The formula last set; the identity after a table or a restore. */
int SLSGetDisplayTransferByFormula(SLSDisplayID display, SLSGammaFormula *formula);
/* Every display back to its default (identity) transfer. */
int SLSRestoreDisplayTransfers(void);

/* ---- Display fades ---------------------------------------------------- */

/*
 * One process at a time holds a fade reservation, for at most
 * kSLSMaxFadeReservation seconds; while it is valid it may fade the
 * displays to a color. A blend is 0 (no color) to 1 (only the color).
 * The reservation and the fade end when the process exits.
 */
#define kSLSMaxFadeReservation	15.0

typedef uint32_t SLSFadeReservation;

int SLSAcquireFadeReservation(double seconds, SLSFadeReservation *reservation);
int SLSReleaseFadeReservation(SLSFadeReservation reservation);
/* The server applies endBlend at once; with synchronous set the caller
 * then waits duration seconds, as the fade would take. */
int SLSFadeDisplays(SLSFadeReservation reservation, double duration, float startBlend,
    float endBlend, float red, float green, float blue, int synchronous);
/* The current blend and the reservation holder's PID (0 when none). */
int SLSGetFadeState(float *blend, int32_t *holder);

/* ---- Display capture --------------------------------------------------- */

/*
 * A captured display belongs to one connection: the server puts a
 * shielding window over it, and only that connection may release it.
 * Everything a connection captured is released when its process exits.
 * display 0 captures or releases every display.
 */
int SLSCaptureDisplay(SLSDisplayID display, uint32_t options);
int SLSReleaseDisplay(SLSDisplayID display);
/* The PID of the process that captured the display, or 0. */
int SLSDisplayCapturedBy(SLSDisplayID display, int32_t *pid);
/* The shielding window over a captured display, or 0. */
uint32_t SLSShieldingWindowID(SLSDisplayID display);
/* The level of shielding windows, as the public CGShieldingWindowLevel. */
#define kSLSShieldingWindowLevel	2147483630

/* ---- Cursor ------------------------------------------------------------ */

typedef struct SLSPoint {
	double	x, y;
} SLSPoint;

/*
 * Each connection keeps its own hide count; the cursor shows when every
 * count is 0. A connection's count is dropped when its process exits.
 */
int SLSHideCursor(SLSDisplayID display);
int SLSShowCursor(SLSDisplayID display);
int SLSCursorIsVisible(int *visible);
/* The cursor position in global coordinates. */
int SLSGetCursorPosition(SLSPoint *point);
/* Moves the cursor (clamped to the displays) without an event. */
int SLSWarpCursorPosition(SLSPoint point);
/* Whether mouse motion moves the cursor, for this connection. */
int SLSAssociateMouseAndCursorPosition(int connected);
/* The motion since the last mouse event (0 after a warp). */
int SLSGetLastMouseDelta(int32_t *dx, int32_t *dy);

/* ---- Windows ----------------------------------------------------------- */

typedef uint32_t SLSWindowID;

typedef struct SLSRect {
	double	x, y, width, height;
} SLSRect;

/* SLSOrderWindow's place, as the public NSWindowOrderingMode. */
enum {
	kSLSOrderOut	= 0,
	kSLSOrderAbove	= 1,
	kSLSOrderBelow	= -1,
};

/* SLSWindowListCopy's option: the public CGWindowListOption bits. */
enum {
	kSLSWindowListAll			= 0,
	kSLSWindowListOnScreenOnly		= 1 << 0,
	kSLSWindowListOnScreenAboveWindow	= 1 << 1,
	kSLSWindowListOnScreenBelowWindow	= 1 << 2,
	kSLSWindowListIncludingWindow		= 1 << 3,
	kSLSWindowListExcludeDesktopElements	= 1 << 4,
};

/* Windows at or below this level are desktop elements. */
#define kSLSDesktopIconWindowLevel	(-2147483603)

/* SLSWindowInfo.flags */
enum {
	kSLSWindowOnScreen	= 1 << 0,
};

/* What the server knows about a window (CGWindowListCopyWindowInfo). */
typedef struct SLSWindowInfo {
	SLSWindowID	window;
	int32_t		level;
	int32_t		ownerPID;
	SLSConnectionID	connection;
	double		x, y, width, height;	/* global coordinates */
	double		alpha;
	uint32_t	flags;			/* kSLSWindow* */
	uint32_t	sharingState;		/* the public CGWindowSharingType */
	char		ownerName[64];
} SLSWindowInfo;

#define kSLSWindowInfoWords	32
#define kSLSMaxWindowList	1024
#ifndef __cplusplus
_Static_assert(sizeof(SLSWindowInfo) == kSLSWindowInfoWords * 4, "SLSWindowInfo size");
#endif

/* A window of this connection, ordered out. */
int SLSNewWindow(int32_t level, SLSRect frame, SLSWindowID *window);
/* The calls below change only the connection's own windows. */
int SLSReleaseWindow(SLSWindowID window);
int SLSSetWindowFrame(SLSWindowID window, SLSRect frame);
int SLSSetWindowLevel(SLSWindowID window, int32_t level);
/* place is kSLSOrder*; relativeTo 0 is the front (or back) of its level. */
int SLSOrderWindow(SLSWindowID window, int32_t place, SLSWindowID relativeTo);
/* Window IDs front to back, as CGWindowListCreate. */
int SLSWindowListCopy(uint32_t option, SLSWindowID relativeTo, uint32_t max,
    SLSWindowID *windows, uint32_t *count);
int SLSGetWindowInfo(SLSWindowID window, SLSWindowInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* _SKYLIGHT_H */
