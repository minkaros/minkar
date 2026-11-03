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
 * cgwindow-test: Quartz Window Services through the new window server
 * (WindowServer plan, WS4a): CGWindowListCopyWindowInfo with the reference
 * keys, CGWindowListCreate, and CGWindowListCreateImage for one window
 * (minimized too, as the Dock's thumbnails) and for an area with two;
 * CGDisplayCreateImage, and a captured display drawn through
 * CGDisplayGetDrawingContext; then Quartz Display Services: finding
 * displays, parameters, modes, configuration, the cursor, gamma, fades.
 * One line per check, "ok" or "FAIL", then "cgwindow-test: N passed, M
 * failed"; the exit status is the number of failures. Needs the new
 * server up.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <SkyLight/SkyLight.h>
#include <IOSurface/IOSurfaceRef.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int passed, failed;

static void
check(int good, const char *what)
{
	printf("%-4s %s\n", good ? "ok" : "FAIL", what);
	if (good)
		passed++;
	else
		failed++;
}

/* A window of one colour (ARGB), on screen. */
static SLSWindowID
solid(SLSRect frame, uint32_t argb)
{
	SLSWindowID w = 0;
	void *pixels;
	uint32_t width, height, rowBytes;

	SLSNewWindow(0, frame, &w);
	SLSWindowBackBuffer(w, &pixels, &width, &height, &rowBytes);
	for (uint32_t y = 0; y < height; y++)
		for (uint32_t x = 0; x < width; x++)
			((uint32_t *)((uint8_t *)pixels + y * rowBytes))[x] = argb;
	SLSWindowCommit(w, (SLSIntRect[]){ { 0, 0, (int32_t)width, (int32_t)height } }, 1);
	SLSOrderWindow(w, kSLSOrderAbove, 0);
	return w;
}

/* A pixel of an IOSurface (BGRA) as ARGB. */
static uint32_t
surfacePixel(IOSurfaceRef s, size_t x, size_t y)
{
	const uint8_t *p = (const uint8_t *)IOSurfaceGetBaseAddress(s) +
	    y * IOSurfaceGetBytesPerRow(s) + x * 4;
	return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

/* CGDisplayStream: frames of the windows' area, half size, on a queue and
 * on a run loop. */
static void
displayStreams(CGDirectDisplayID main)
{
	__block int frames = 0, stopped = 0;
	__block uint32_t redSeen = 0, blueSeen = 0, inUse = 0;
	__block size_t rectCount = 0;
	__block CGRect firstRect = CGRectNull;
	dispatch_semaphore_t got = dispatch_semaphore_create(0);
	CGRect source = CGRectMake(100, 120, 160, 120);
	CFDictionaryRef sourceDict = CGRectCreateDictionaryRepresentation(source);
	const void *keys[] = { kCGDisplayStreamSourceRect, kCGDisplayStreamShowCursor };
	const void *values[] = { sourceDict, kCFBooleanFalse };
	CFDictionaryRef props = CFDictionaryCreate(NULL, keys, values, 2,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	dispatch_queue_t q = dispatch_queue_create("cgwindow-test.stream", NULL);

	CGDisplayStreamRef stream = CGDisplayStreamCreateWithDispatchQueue(main, 80, 60, 'BGRA',
	    props, q, ^(CGDisplayStreamFrameStatus status, uint64_t time, IOSurfaceRef surface,
	    CGDisplayStreamUpdateRef update) {
		(void)time;
		if (status == kCGDisplayStreamFrameStatusStopped) {
			stopped++;
			dispatch_semaphore_signal(got);
			return;
		}
		if (status != kCGDisplayStreamFrameStatusFrameComplete || surface == NULL)
			return;
		frames++;
		if (frames == 1) {
			redSeen = surfacePixel(surface, 5, 5);
			blueSeen = surfacePixel(surface, 55, 40);
			inUse = IOSurfaceIsInUse(surface);
		} else {
			const CGRect *r = CGDisplayStreamUpdateGetRects(update,
			    kCGDisplayStreamUpdateDirtyRects, &rectCount);
			if (r != NULL && rectCount > 0)
				firstRect = r[0];
		}
		dispatch_semaphore_signal(got);
	});
	check(stream != NULL && CFGetTypeID(stream) == CGDisplayStreamGetTypeID(),
	    "a display stream is made");
	check(stream != NULL && CGDisplayStreamStart(stream) == kCGErrorSuccess,
	    "and started");
	long waited = dispatch_semaphore_wait(got, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
	check(waited == 0 && frames >= 1, "its first frame comes");
	check(redSeen == 0xffff0000 && blueSeen == 0xff0000ff,
	    "with the windows, from the source rect scaled to the output size");
	check(inUse, "the surface is the handler's while it runs");

	/* A window appears in the source: a frame with what changed. */
	SLSWindowID green = solid((SLSRect){ 110, 130, 10, 10 }, 0xff00ff00);
	waited = dispatch_semaphore_wait(got, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
	check(waited == 0 && frames >= 2 && rectCount >= 1 &&
	    CGRectContainsRect(firstRect, CGRectMake(5, 5, 5, 5)),
	    "a change comes as a frame with its rect, in output pixels");
	SLSReleaseWindow(green);

	check(stream != NULL && CGDisplayStreamStop(stream) == kCGErrorSuccess,
	    "the stream stops");
	waited = dispatch_semaphore_wait(got, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
	check(waited == 0 && stopped == 1, "and the handler hears it stopped");
	check(CGDisplayStreamGetRunLoopSource(stream) == NULL, "a queue's stream has no run loop source");
	if (stream != NULL)
		CFRelease(stream);

	/* The run loop variant. */
	__block int loopFrames = 0;
	CGDisplayStreamRef loop = CGDisplayStreamCreate(main, 80, 60, 'BGRA', props,
	    ^(CGDisplayStreamFrameStatus status, uint64_t time, IOSurfaceRef surface,
	    CGDisplayStreamUpdateRef update) {
		(void)time; (void)surface; (void)update;
		if (status == kCGDisplayStreamFrameStatusFrameComplete)
			loopFrames++;
	});
	CFRunLoopSourceRef rls = loop ? CGDisplayStreamGetRunLoopSource(loop) : NULL;
	if (rls != NULL) {
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		CGDisplayStreamStart(loop);
		for (int i = 0; i < 30 && loopFrames == 0; i++)
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
		CGDisplayStreamStop(loop);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.2, false);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	}
	check(rls != NULL && loopFrames >= 1, "a run loop's stream delivers on its run loop");
	if (loop != NULL)
		CFRelease(loop);
	check(CGDisplayStreamCreate(main, 80, 60, '420v', NULL, ^(CGDisplayStreamFrameStatus st,
	    uint64_t t, IOSurfaceRef su, CGDisplayStreamUpdateRef u) { (void)st; (void)t; (void)su;
	    (void)u; }) == NULL, "a format it cannot make is refused");
	CFRelease(props);
	CFRelease(sourceDict);
}

/* The ARGB value of an image's pixel (x, y from the top left). */
static uint32_t
pixel(CGImageRef image, size_t x, size_t y)
{
	CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));
	uint32_t value = 0;

	if (data == NULL)
		return 0;
	const uint8_t *p = CFDataGetBytePtr(data) + y * CGImageGetBytesPerRow(image) + x * 4;
	value = (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
	CFRelease(data);
	return value;
}

static int
dictionaryHasNumber(CFDictionaryRef d, CFStringRef key, int64_t want)
{
	CFNumberRef n = CFDictionaryGetValue(d, key);
	int64_t v = 0;

	return n != NULL && CFNumberGetValue(n, kCFNumberSInt64Type, &v) && v == want;
}

/*
 * Quartz Display Services through the window server (WS4b, without the
 * legacy RPC): finding displays, their parameters, modes, configuration,
 * the cursor, gamma and fades.
 */
static void
displays(CGDirectDisplayID main)
{
	CGDirectDisplayID found[4];
	uint32_t n = 0;

	check(CGGetDisplaysWithPoint(CGPointMake(10, 10), 4, found, &n) == kCGErrorSuccess &&
	    n == 1 && found[0] == main, "CGGetDisplaysWithPoint finds the main display");
	n = 0;
	check(CGGetDisplaysWithRect(CGRectMake(0, 0, 50, 50), 4, found, &n) == kCGErrorSuccess &&
	    n == 1 && found[0] == main, "and CGGetDisplaysWithRect");
	CGOpenGLDisplayMask mask = CGDisplayIDToOpenGLDisplayMask(main);
	check(mask != 0 && CGOpenGLDisplayMaskToDisplayID(mask) == main,
	    "its OpenGL display mask, both ways");
	CGSize mm = CGDisplayScreenSize(main);
	check(mm.width > 0 && mm.height > 0, "its physical size");
	check(CGDisplayIsActive(main) && CGDisplayIsOnline(main) && CGDisplayIsMain(main),
	    "it is active, online and main");
	check(CGDisplayMirrorsDisplay(main) == kCGNullDirectDisplay &&
	    CGDisplayPrimaryDisplay(main) == main && CGDisplayRotation(main) == 0,
	    "no mirror, its own primary, not rotated");

	/* Modes: the current one is among them, and setting it works. */
	CGDisplayModeRef current = CGDisplayCopyDisplayMode(main);
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(main, NULL);
	int listed = 0;
	for (CFIndex i = 0; modes != NULL && i < CFArrayGetCount(modes); i++) {
		CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		if (current != NULL &&
		    CGDisplayModeGetIODisplayModeID(m) == CGDisplayModeGetIODisplayModeID(current) &&
		    CGDisplayModeGetWidth(m) == CGDisplayModeGetWidth(current))
			listed = 1;
	}
	check(current != NULL && modes != NULL && CFArrayGetCount(modes) > 0 && listed,
	    "the current mode is among the display's modes");
	check(current != NULL && CGDisplaySetDisplayMode(main, current, NULL) == kCGErrorSuccess,
	    "setting the current mode again succeeds");
	if (modes != NULL)
		CFRelease(modes);
	CGDisplayModeRelease(current);

	/* A configuration begun and cancelled, and one completed with nothing. */
	CGDisplayConfigRef config;
	check(CGBeginDisplayConfiguration(&config) == kCGErrorSuccess &&
	    CGConfigureDisplayOrigin(config, main, 0, 0) == kCGErrorSuccess &&
	    CGCancelDisplayConfiguration(config) == kCGErrorSuccess,
	    "a display configuration, cancelled");
	check(CGBeginDisplayConfiguration(&config) == kCGErrorSuccess &&
	    CGCompleteDisplayConfiguration(config, kCGConfigureForSession) == kCGErrorSuccess,
	    "and one completed");

	/* The cursor: warped, it is where it was put; hidden and shown. */
	CGWarpMouseCursorPosition(CGPointMake(321, 234));
	SLSPoint p = { 0, 0 };
	check(SLSGetCursorPosition(&p) == kSLSErrorSuccess && p.x == 321 && p.y == 234,
	    "CGWarpMouseCursorPosition moves the cursor");
	CGDisplayMoveCursorToPoint(main, CGPointMake(100, 50));
	check(SLSGetCursorPosition(&p) == kSLSErrorSuccess && p.x == 100 && p.y == 50,
	    "and CGDisplayMoveCursorToPoint, from the display's top left");
	int visible = 1;
	check(CGDisplayHideCursor(main) == kCGErrorSuccess &&
	    SLSCursorIsVisible(&visible) == kSLSErrorSuccess && !visible,
	    "CGDisplayHideCursor hides it");
	check(CGDisplayShowCursor(main) == kCGErrorSuccess &&
	    SLSCursorIsVisible(&visible) == kSLSErrorSuccess && visible, "and it shows again");
	int32_t dx = 7, dy = 7;
	CGGetLastMouseDelta(&dx, &dy);
	check(dx == 0 && dy == 0, "no mouse delta after a warp");

	/* Gamma: the capacity, a formula set and read back, then restored. */
	check(CGDisplayGammaTableCapacity(main) == 256, "the gamma table capacity");
	CGGammaValue v[9];
	check(CGSetDisplayTransferByFormula(main, 0, 1, 2, 0, 1, 2, 0, 1, 2) == kCGErrorSuccess &&
	    CGGetDisplayTransferByFormula(main, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
	    &v[7], &v[8]) == kCGErrorSuccess && v[2] == 2 && v[5] == 2 && v[8] == 2,
	    "a transfer formula, set and read back");
	uint8_t bytes[2] = { 0, 255 };
	CGGammaValue r[256], g[256], b[256];
	uint32_t samples = 0;
	check(CGSetDisplayTransferByByteTable(main, 2, bytes, bytes, bytes) == kCGErrorSuccess &&
	    CGGetDisplayTransferByTable(main, 256, r, g, b, &samples) == kCGErrorSuccess &&
	    samples > 0, "a byte table, set and read back");
	CGDisplayRestoreColorSyncSettings();

	/* A fade reservation, a fade at once, released. */
	CGDisplayFadeReservationToken token;
	check(CGAcquireDisplayFadeReservation(5, &token) == kCGErrorSuccess &&
	    CGDisplayFade(token, 0, kCGDisplayBlendNormal, kCGDisplayBlendNormal, 0, 0, 0, true) ==
	    kCGErrorSuccess && CGReleaseDisplayFadeReservation(token) == kCGErrorSuccess,
	    "a fade reservation, a fade and its release");
}

int
main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	SLSWindowID red = solid((SLSRect){ 100, 120, 80, 60 }, 0xffff0000);
	SLSWindowID blue = solid((SLSRect){ 140, 150, 80, 60 }, 0xff0000ff);

	/* The list and the descriptions. */
	CFArrayRef ids = CGWindowListCreate(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
	check(ids != NULL && CFArrayGetCount(ids) >= 2 &&
	    (SLSWindowID)(uintptr_t)CFArrayGetValueAtIndex(ids, 0) == blue,
	    "CGWindowListCreate lists the windows front to back");
	if (ids != NULL)
		CFRelease(ids);
	CFArrayRef info = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, red);
	CFDictionaryRef d = (info != NULL && CFArrayGetCount(info) == 1) ?
	    CFArrayGetValueAtIndex(info, 0) : NULL;
	CGRect bounds = CGRectZero;
	check(d != NULL && dictionaryHasNumber(d, kCGWindowNumber, red) &&
	    dictionaryHasNumber(d, kCGWindowOwnerPID, getpid()) &&
	    dictionaryHasNumber(d, kCGWindowLayer, 0) &&
	    CFDictionaryGetValue(d, kCGWindowIsOnscreen) == kCFBooleanTrue &&
	    CFDictionaryGetValue(d, kCGWindowOwnerName) != NULL &&
	    CGRectMakeWithDictionaryRepresentation(CFDictionaryGetValue(d, kCGWindowBounds),
	        &bounds) && CGRectEqualToRect(bounds, CGRectMake(100, 120, 80, 60)),
	    "a window's description has the reference keys");
	if (info != NULL)
		CFRelease(info);

	/* One window's image. */
	CGImageRef image = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow,
	    red, kCGWindowImageBoundsIgnoreFraming);
	check(image != NULL && CGImageGetWidth(image) == 80 && CGImageGetHeight(image) == 60 &&
	    pixel(image, 10, 10) == 0xffff0000, "one window's image");
	if (image != NULL)
		CGImageRelease(image);

	/* Minimized, as the Dock's thumbnail. */
	SLSSetWindowMinimized(blue, 1);
	image = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, blue,
	    kCGWindowImageDefault);
	check(image != NULL && CGImageGetWidth(image) == 80 && pixel(image, 40, 30) == 0xff0000ff,
	    "a minimized window's image");
	if (image != NULL)
		CGImageRelease(image);
	SLSSetWindowMinimized(blue, 0);

	/* An area with both: blue over red where they meet. */
	image = CGWindowListCreateImage(CGRectMake(100, 120, 120, 90),
	    kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
	check(image != NULL && CGImageGetWidth(image) == 120 && CGImageGetHeight(image) == 90 &&
	    pixel(image, 5, 5) == 0xffff0000 && pixel(image, 60, 50) == 0xff0000ff &&
	    pixel(image, 110, 5) == 0,
	    "an area's image composes the windows in order");
	if (image != NULL)
		CGImageRelease(image);

	/* The display's image: the windows over black. */
	CGDirectDisplayID main = CGMainDisplayID();
	image = CGDisplayCreateImageForRect(main, CGRectMake(90, 110, 100, 80));
	check(image != NULL && CGImageGetWidth(image) == 100 && pixel(image, 20, 20) == 0xffff0000 &&
	    pixel(image, 2, 2) == 0xff000000, "a display's image has its windows over black");
	if (image != NULL)
		CGImageRelease(image);

	displayStreams(main);

	/* Captured, the display shows what the app draws in its context. */
	check(CGDisplayCapture(main) == kCGErrorSuccess, "the display is captured");
	CGContextRef context = CGDisplayGetDrawingContext(main);
	check(context != NULL, "its drawing context");
	if (context != NULL) {
		CGContextSetRGBFillColor(context, 0, 1, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, 4000, 4000));
	}
	usleep(200000);	/* a frame or two: the server shows the context's surface */
	image = CGDisplayCreateImageForRect(main, CGRectMake(10, 10, 20, 20));
	check(image != NULL && pixel(image, 5, 5) == 0xff00ff00, "what it draws reaches the screen");
	if (image != NULL)
		CGImageRelease(image);
	/* Drawn again, with no flush: the surface is shown at every frame. */
	if (context != NULL) {
		CGContextSetRGBFillColor(context, 1, 1, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, 4000, 4000));
	}
	usleep(200000);
	image = CGDisplayCreateImageForRect(main, CGRectMake(10, 10, 20, 20));
	check(image != NULL && pixel(image, 5, 5) == 0xffffff00, "and what it draws next, with no flush");
	if (image != NULL)
		CGImageRelease(image);
	check(CGDisplayRelease(main) == kCGErrorSuccess, "and it is released");
	image = CGDisplayCreateImageForRect(main, CGRectMake(90, 110, 100, 80));
	check(image != NULL && pixel(image, 20, 20) == 0xffff0000, "the windows show again");
	if (image != NULL)
		CGImageRelease(image);

	SLSReleaseWindow(blue);
	SLSReleaseWindow(red);
	displays(main);
	printf("cgwindow-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
