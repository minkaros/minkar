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
#include <sys/types.h>
#include <mach/mach.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The window server's Mach service (the legacy server's name until
 * WS4b, when this one replaced it). */
#define kSLSServiceName		"org.minkar.WindowServer"

/* The interface version both sides are built for. */
#define kSLSInterfaceVersion	18

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

/*
 * The menu bar (interface version 6): a strip at the top of every display,
 * kept for the front app's menu bar and the status area. Its height is in
 * points; windows the server moves stay below it.
 */
#define kSLSMenuBarHeight	22
int SLSGetDisplayMenuBarHeight(SLSDisplayID display, uint32_t *height);


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
/* Every display as last saved permanently, or as the server started when
 * nothing was (interface version 15). */
int SLSRestorePermanentDisplayConfiguration(void);

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
/* The server goes from startBlend to endBlend over duration seconds (at
 * once for 0), drawing each frame; with synchronous set the caller waits
 * that long. Interface version 14 sends the duration. */
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
/* The menu levels, as the public kCGMainMenuWindowLevel,
 * kCGStatusWindowLevel and kCGPopUpMenuWindowLevel. */
/* The Dock's level, as the public kCGDockWindowLevel. */
#define kSLSDockWindowLevel		20
#define kSLSMainMenuWindowLevel		24
#define kSLSStatusWindowLevel		25
#define kSLSPopUpMenuWindowLevel	101

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

/*
 * The cursor's image (interface version 12), as NSCursor sets it: each
 * connection has its own, and the screen shows the front app's (the
 * server's arrow while it set none, or with a NULL image).
 * Premultiplied BGRA, width * 4 bytes a row, at most
 * kSLSMaxCursorSize on a side; the hot spot is from its top left.
 */
#define kSLSMaxCursorSize	256
int SLSSetCursorImage(const uint8_t *pixels, uint32_t width, uint32_t height, double hotX,
    double hotY);
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
	kSLSWindowMinimized	= 1 << 1,
	kSLSWindowHasShadow	= 1 << 2,
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

/*
 * Status items (interface version 8): a window of its app made a status
 * item is laid out by the server in the main display's menu bar, right to
 * left in the order the items came, as wide as its frame; the server
 * places it and tells its app (kSLSEventWindowMoved). Shown items only
 * take room. A window at the status level that is not a status item is
 * left where its app puts it.
 */
int SLSSetWindowStatusItem(SLSWindowID window, int isItem);

/*
 * Minimized windows (interface version 9), for the Dock, which keeps them
 * as on the reference platform: its app minimizes a window, the server
 * orders it out (another window of the app becomes key) and tells the
 * connections that asked. Restoring, or ordering the window in again,
 * puts it in front.
 */
int SLSSetWindowMinimized(SLSWindowID window, int minimized);
int SLSRequestWindowNotifications(int on);

/*
 * A window's image (interface version 10): what its surface shows,
 * premultiplied BGRA, *rowBytes apart, also for a minimized or ordered-out
 * window (the Dock's thumbnails, CGWindowListCreateImage). free(*pixels)
 * when done. Its app may read it; another process only under the
 * built-in policy of event taps (SHORTCUT(M5), until the privacy
 * service's screen recording permission).
 */
int SLSCopyWindowImage(SLSWindowID window, uint8_t **pixels, uint32_t *width,
    uint32_t *height, uint32_t *rowBytes);

/*
 * Child windows (interface version 11), for NSWindow's child windows and
 * sheets: a window of the same app attached to a parent, place
 * kSLSOrderAbove or kSLSOrderBelow it. The server keeps the child next to
 * its parent in the order, moves it with its parent (telling its app,
 * kSLSEventWindowMoved), and orders it out and back in with its parent
 * (minimizing too). parent 0 detaches it; a parent that goes away
 * detaches its children.
 */
int SLSSetWindowParent(SLSWindowID child, SLSWindowID parent, int32_t place);

/*
 * Window shadows (interface version 13), as NSWindow's hasShadow: the
 * server draws a soft shadow under the window, a little below it, in
 * software. Off for a new window.
 */
int SLSSetWindowShadow(SLSWindowID window, int hasShadow);

/* 0 (transparent) to 1 (opaque); the server blends the surface with it. */
int SLSSetWindowAlpha(SLSWindowID window, double alpha);

/* ---- Window surfaces ---------------------------------------------------- */

typedef struct SLSIntRect {
	int32_t	x, y, width, height;
} SLSIntRect;

/*
 * A window's surface: two buffers of premultiplied 32-bit BGRA pixels
 * (alpha in the high byte), its frame's size rounded up, in memory the
 * server shares with this process only. The client draws into the back
 * buffer and commits it with the rectangles it changed (window
 * coordinates, origin top left); the server shows it from its next frame,
 * and the other buffer becomes the back one, already holding the same
 * pixels. A surface is made on first use and again after the window's
 * size changes; until its first commit the window shows nothing.
 */
int SLSWindowBackBuffer(SLSWindowID window, void **pixels, uint32_t *width,
    uint32_t *height, uint32_t *rowBytes);
int SLSWindowCommit(SLSWindowID window, const SLSIntRect *damage, uint32_t count);
/* Copies the damaged rectangles from pixels (the window's size, rowBytes
 * apart) into the back buffer, then commits. */
int SLSWindowFlushPixels(SLSWindowID window, const void *pixels, uint32_t rowBytes,
    const SLSIntRect *damage, uint32_t count);

/*
 * The window shows an IOSurface of this process (its ID; BGRA, 4 bytes per
 * pixel, the window's size) in place of its own buffers: what the app
 * draws into it shows at the next frame, with no commit, as on a display's
 * scanout (CGDisplayGetDrawingContext). Surface 0 gives the window buffers
 * of its own again (interface version 15).
 */
int SLSWindowSetIOSurface(SLSWindowID window, uint32_t surfaceID);

/*
 * A layer of the window (CAMetalLayer's): an IOSurface of this process
 * (BGRA, 4 bytes per pixel, rect's size) shown over the window's own
 * content in rect (window coordinates, inside the window), until another
 * is set; it moves, orders and fades with the window, and events in it go
 * to the window. Each call shows the surface at the next frame (a
 * present); surface 0 takes the layer away (interface version 16).
 */
int SLSWindowSetLayerIOSurface(SLSWindowID window, uint32_t surfaceID, SLSIntRect rect);

/*
 * Pacing a layer by the display: once a frame that shows the window's
 * last layer surface is on the screen (after its vertical blank where the
 * display flips), the server sends notify an SLSLayerShown, without
 * waiting (a full queue drops it). MACH_PORT_NULL stops it.
 */
#define kSLSLayerShownMessageID	0x534c4c53	/* 'SLLS' */
typedef struct SLSLayerShown {
	mach_msg_header_t	header;
	uint32_t		window;
	uint32_t		surface;	/* the layer surface shown */
	uint64_t		time;		/* mach_absolute_time, once shown */
} SLSLayerShown;
int SLSWindowSetLayerNotifyPort(SLSWindowID window, mach_port_t notify);

#define kSLSMaxSurfaceSide	16384
#define kSLSMaxDamageRects	64

/* ---- Display streams (interface version 15) ---------------------------- */

/*
 * A display stream copies a display's frames into IOSurfaces of the
 * caller (CGDisplayStream). After a frame that changed the display, the
 * server copies it, from source scaled to the output size, into a surface
 * whose use count is 0, increments that count and sends the caller's port
 * an SLSDisplayStreamFrame; the caller decrements it when done.
 */
typedef struct SLSDisplayStreamOptions {
	uint32_t	outputWidth, outputHeight;
	int32_t		sourceX, sourceY;	/* display pixels; width 0: all of it */
	uint32_t	sourceWidth, sourceHeight;
	uint32_t	minFrameMicros;		/* 0: every frame */
	uint32_t	showCursor;
	uint32_t	reserved[2];
} SLSDisplayStreamOptions;

#ifndef __cplusplus
_Static_assert(sizeof(SLSDisplayStreamOptions) == 10 * 4, "SLSDisplayStreamOptions size");
#endif

#define kSLSMaxStreamRects	16
#define kSLSDisplayStreamMessageID	0x534c4453	/* 'SLDS' */

enum {
	kSLSDisplayStreamFrameComplete	= 0,
	kSLSDisplayStreamStopped	= 3,
};

/* What the server sends: a frame in a surface, or that the stream stopped.
 * The rects are what changed since the last frame sent, in output pixels
 * (more than kSLSMaxStreamRects: the box around them all). */
typedef struct SLSDisplayStreamFrame {
	mach_msg_header_t	header;
	uint32_t		stream;
	uint32_t		status;		/* kSLSDisplayStream* */
	uint32_t		surface;	/* IOSurfaceID; 0 when stopped */
	uint32_t		count;
	uint64_t		time;		/* mach_absolute_time of the frame */
	SLSIntRect		rects[kSLSMaxStreamRects];
} SLSDisplayStreamFrame;

int SLSDisplayStreamStart(SLSDisplayID display, const uint32_t *surfaces, uint32_t count,
    const SLSDisplayStreamOptions *options, mach_port_t notify, uint32_t *stream);
int SLSDisplayStreamStop(uint32_t stream);

/* ---- Events ------------------------------------------------------------ */

/* SLSEvent.type: the public CGEventType values (the same as NSEventType's). */
enum {
	kSLSEventLeftMouseDown		= 1,
	kSLSEventLeftMouseUp		= 2,
	kSLSEventRightMouseDown		= 3,
	kSLSEventRightMouseUp		= 4,
	kSLSEventMouseMoved		= 5,
	kSLSEventLeftMouseDragged	= 6,
	kSLSEventRightMouseDragged	= 7,
	kSLSEventKeyDown		= 10,
	kSLSEventKeyUp			= 11,
	kSLSEventFlagsChanged		= 12,
	kSLSEventScrollWheel		= 22,
	kSLSEventOtherMouseDown		= 25,
	kSLSEventOtherMouseUp		= 26,
	kSLSEventOtherMouseDragged	= 27,
	/* Private: a server-side move put the window at x, y (its frame's
	 * top left); sent as it goes and when the move ends (clickCount 1). */
	kSLSEventWindowMoved		= 0x1001,
	/* A symbolic hotkey for its owner: buttonNumber is the hotkey,
	 * keyCode, flags and isRepeat the key that was pressed. */
	kSLSEventSymbolicHotKey		= 0x1002,
	/* To connections that asked (SLSRequestWindowNotifications): a
	 * window was minimized or restored; x, y is its frame's top left.
	 * SLSGetWindowInfo gives its owner. */
	kSLSEventWindowMinimized	= 0x1003,
	kSLSEventWindowRestored		= 0x1004,
	/* To every connection: a display's mode, place or mirroring changed.
	 * buttonNumber is the display, flags the public
	 * CGDisplayChangeSummaryFlags of the change (never the begin flag);
	 * x, y is its new origin and dx, dy its new size, global. */
	kSLSEventDisplayReconfigured	= 0x1005,
};

/*
 * One input event, as the server routes it: mouse events go to the window
 * under the pointer (to the one that got the mouse down until every
 * button is up), keys to the key window. Locations are global (top left
 * origin); flags are the public CGEventFlags.
 */
typedef struct SLSEvent {
	uint32_t	type;
	SLSWindowID	window;			/* the window it was routed to */
	uint32_t	flags;
	uint16_t	keyCode;
	uint16_t	isRepeat;
	int32_t		clickCount;		/* mouse downs and ups */
	uint32_t	buttonNumber;		/* 0 left, 1 right, 2 other */
	double		x, y;
	double		dx, dy;			/* mouse moves */
	double		scrollX, scrollY;	/* lines */
	uint64_t	timestamp;		/* nanoseconds since boot */
	char		characters[8];		/* UTF-8 */
	char		charactersIgnoringModifiers[8];
} SLSEvent;

#define kSLSEventWords		24
#define kSLSMaxEvents		64
#ifndef __cplusplus
_Static_assert(sizeof(SLSEvent) == kSLSEventWords * 4, "SLSEvent size");
#endif

/*
 * This connection's events. The server keeps a bounded queue per
 * connection, mouse moves coalesced, and sends a message to SLSEventPort
 * when the queue stops being empty; SLSReceiveEvents empties the queue,
 * after waiting for that message when wait is set. When the server goes
 * away it returns kSLSErrorInvalidConnection (the wait ends too).
 */
mach_port_t SLSEventPort(void);
int SLSReceiveEvents(SLSEvent *events, uint32_t max, uint32_t *count, int wait);

/*
 * Moves the window with the pointer while the left button stays down: the
 * server moves it by itself (no drags reach the app meanwhile) and sends
 * kSLSEventWindowMoved. Asked for from a mouse down, as
 * -[NSWindow performWindowDragWithEvent:] does.
 */
int SLSWindowStartMove(SLSWindowID window);

/*
 * The window's drag regions, in the window's coordinates (top left
 * origin): a left mouse down inside a drag rectangle and outside every
 * excluded one starts a server-side move at once, without the app (it
 * still gets the mouse down, and the mouse up when the move ends). AppKit
 * gives the title bar without its buttons and, for a window movable by its
 * background, the window without the views that take their own clicks.
 * At most kSLSMaxRegionRects of each; no rectangles: no drag region.
 */
#define kSLSMaxRegionRects	64

/*
 * Symbolic hotkeys: system-wide shortcuts the server recognizes and sends
 * to their owner as kSLSEventSymbolicHotKey, instead of delivering the key
 * to the key window. The defaults below can be changed in
 * /Library/Preferences/org.minkar.symbolichotkeys.plist, a dictionary
 * "AppleSymbolicHotKeys" of hotkey number -> { enabled; value = {
 * parameters = (character, key code, modifier flags); type = standard } }.
 * A hotkey with no owner is still not delivered as a key.
 */
enum {
	kSLSHotKeyAppSwitcher		= 1,	/* Command-Tab: the Dock */
	kSLSHotKeyAppSwitcherBackward	= 2,	/* Command-Shift-Tab: the Dock */
	kSLSHotKeyForceQuit		= 3,	/* Command-Option-Escape: loginwindow */
	kSLSHotKeyMenuBarFocus		= 4,	/* Control-F2: the front app */
	kSLSMaxSymbolicHotKeys		= 4
};
/* The connection becomes (claim) or stops being the hotkey's owner. The
 * front app owns kSLSHotKeyMenuBarFocus, and cannot be claimed. */
int SLSClaimSymbolicHotKey(uint32_t hotKey, int claim);
/* The hotkey's key code, modifier flags (NSEventModifierFlags) and state. */
int SLSGetSymbolicHotKey(uint32_t hotKey, uint16_t *keyCode, uint32_t *modifiers,
    int *enabled);
int SLSWindowSetDragRegion(SLSWindowID window, const SLSIntRect *drag, uint32_t dragCount,
    const SLSIntRect *exclude, uint32_t excludeCount);

/* ---- Event taps and posted events ------------------------------------- */

/*
 * A tap sees the events of the types in its mask (bit 1 << type) before
 * they are routed: taps at the HID location (0) see every event, session
 * taps (1 and 2) the events entering at the session location too. A
 * listen-only tap is sent a copy; a filtering one is asked, in its turn,
 * and may change the event or drop it; one that does not answer within a
 * second is disabled. Taps are sent to port (a receive right of the
 * caller's) as SLSTapEventMessages (read by CoreGraphics' CGEventTapCreate).
 * SHORTCUT(M5): filtering taps, and listen-only taps that see keys, need
 * root or the console's user until the privacy service decides
 * (WindowServer plan, section 9); taken in WS3.
 */
typedef struct SLSEventTapInfo {
	uint64_t	mask;
	uint32_t	tap;
	uint32_t	location;		/* CGEventTapLocation */
	uint32_t	options;		/* CGEventTapOptions */
	int32_t		tappingPID;
	int32_t		processBeingTapped;	/* 0: every process */
	uint32_t	enabled;
	float		minUsecLatency, avgUsecLatency, maxUsecLatency;
	uint32_t	reserved;
} SLSEventTapInfo;

#define kSLSEventTapInfoWords	12
#define kSLSMaxEventTaps	32
#ifndef __cplusplus
_Static_assert(sizeof(SLSEventTapInfo) == kSLSEventTapInfoWords * 4, "SLSEventTapInfo size");
#endif

/*
 * An event for a tap, sent to the tap's port. For a filtering tap the
 * message carries a send-once reply port, and the tap answers with an
 * SLSTapReplyMessage (drop 1 to drop the event; event, possibly changed).
 */
#define kSLSTapEventMessageID	0x534c5354	/* 'SLST' */
#define kSLSTapReplyMessageID	0x534c5352	/* 'SLSR' */

typedef struct SLSTapEventMessage {
	mach_msg_header_t	header;
	uint32_t		tap;
	uint32_t		reserved;
	SLSEvent		event;
} SLSTapEventMessage;

typedef struct SLSTapReplyMessage {
	mach_msg_header_t	header;
	uint32_t		drop;
	uint32_t		reserved;
	SLSEvent		event;
} SLSTapReplyMessage;

/* Sent to a tap the server disabled (the public kCGEventTapDisabledByTimeout). */
#define kSLSEventTapDisabledByTimeout	0xFFFFFFFE

int SLSEventTapCreate(uint32_t location, uint32_t placement, uint32_t options,
    uint64_t mask, mach_port_t port, uint32_t *tap);
int SLSEventTapEnable(uint32_t tap, int enable);
int SLSEventTapRelease(uint32_t tap);
int SLSGetEventTapList(uint32_t max, SLSEventTapInfo *taps, uint32_t *count);
/* Posts an event at a tap location: it passes the taps from there and is
 * routed as input (mouse events move the cursor). SHORTCUT(M5): root or
 * the console's user only, as filtering taps, until the privacy service
 * decides; taken in WS3. */
int SLSPostEvent(uint32_t location, const SLSEvent *event);

/*
 * The input state (interface version 14), for CGEventSource's state
 * functions: kSLSHIDStateHardware is what the input devices did,
 * kSLSHIDStateCombined adds the events posted (SLSPostEvent). Buttons are
 * bits by button number, keys bits by key code; per event type (the
 * public CGEventType, below 32), how many there were and when the last
 * one came (nanoseconds since boot, as now).
 */
enum {
	kSLSHIDStateHardware	= 0,
	kSLSHIDStateCombined	= 1,
};
#define kSLSHIDEventTypes	32
typedef struct SLSHIDState {
	uint32_t	buttons;
	uint32_t	flags;
	uint32_t	keys[4];		/* key codes 0 to 127 */
	uint32_t	counters[kSLSHIDEventTypes];
	uint64_t	lastEvent[kSLSHIDEventTypes];
	uint64_t	now;
} SLSHIDState;
#define kSLSHIDStateWords	104
#ifndef __cplusplus
_Static_assert(sizeof(SLSHIDState) == kSLSHIDStateWords * 4, "SLSHIDState size");
#endif
int SLSGetHIDState(uint32_t which, SLSHIDState *state);

/* The key window, which gets the keys; a click makes a window key too. */
int SLSSetKeyWindow(SLSWindowID window);
SLSWindowID SLSGetKeyWindow(void);

/*
 * The front app (interface version 5): the process whose menus the menu
 * bar shows and whose key window gets the keys. Making a window key (a
 * click, or SLSSetKeyWindow) makes its owner the front app;
 * SLSSetFrontProcess makes the caller the front app, with or without a
 * window, and its front window key. When the front app goes away, the one
 * in front before it takes its place. LaunchServices' session is told.
 */
int SLSSetFrontProcess(void);
/* The front app's process, or 0 when there is none. */
int SLSGetFrontProcess(pid_t *pid);

/*
 * Pointer capture (interface version 7), for an open menu: every mouse
 * event goes to the window, wherever the pointer is, and no click makes a
 * window key or starts a move. Only the front app may capture, with one
 * of its windows; 0 releases. It ends by itself when the window or its
 * app goes away.
 */
int SLSCapturePointer(SLSWindowID window);
/* The window holding the pointer, or 0. */
SLSWindowID SLSGetPointerCapture(void);

/* ---- Server statistics (wsctl) ----------------------------------------- */

typedef struct SLSServerStats {
	uint64_t	frames;			/* frames composited and presented */
	uint64_t	idleTicks;		/* display link ticks with nothing to do */
	uint64_t	commits;		/* surface commits received */
	uint64_t	damagedPixels;		/* composited, all frames */
	uint64_t	lastDamagedPixels;	/* composited, last frame */
	uint64_t	lastFrameMicros;	/* compositing and present, last frame */
	uint64_t	totalFrameMicros;
	uint64_t	events;			/* input events read from the HID system */
	/* Interface version 17, for measurements (GPU track G4.6): over the
	 * last 1,024 frames, compositing and present; from the first damage
	 * after a frame to its present (lateFrames: over two refreshes); and
	 * from an input event (IOHIDSystem's time) to the present of the
	 * frame after it, the pointer's input-to-present latency. */
	uint64_t	frameMicrosP50, frameMicrosP95, frameMicrosP99, frameMicrosMax;
	uint64_t	damageMicrosP50, damageMicrosP95;
	uint64_t	lateFrames;
	uint64_t	inputMicrosP50, inputMicrosP95, inputSamples;
	uint64_t	refreshMicros;		/* the main display's refresh period */
	/* Compositing alone (the frame drawn, before its present, which with
	 * page flips waits for the vertical blank). */
	uint64_t	composeMicrosP50, composeMicrosP95;
} SLSServerStats;

#define kSLSServerStatsWords	42
#ifndef __cplusplus
_Static_assert(sizeof(SLSServerStats) == kSLSServerStatsWords * 4, "SLSServerStats size");
#endif

int SLSGetServerStats(SLSServerStats *stats);
/* All counters and samples back to zero (for a measurement's start). */
int SLSResetServerStats(void);

/* ---- The keyboard (interface version 18) ----
 * Its layout (XKB names: "us", "es", "latam"; a variant, "" for none) and
 * how a held key repeats: the delay before the first repeat and the
 * interval between the next ones, in milliseconds. The server keeps them
 * (/var/db/WindowServer/Keyboard.plist). Root or the console's user may
 * set them; kSLSErrorIllegalArgument for a layout the server cannot load
 * or times out of 15..2000 ms. System Preferences' Keyboard pane sets
 * them. */
typedef struct {
	char layout[32];
	char variant[32];
	uint32_t initialRepeat;
	uint32_t repeatInterval;
} SLSKeyboardSettings;

int SLSSetKeyboardSettings(const SLSKeyboardSettings *settings);
int SLSGetKeyboardSettings(SLSKeyboardSettings *settings);

#ifdef __cplusplus
}
#endif

#endif /* _SKYLIGHT_H */
