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
 * CGDisplayGetDrawingContext.
 * One line per check, "ok" or "FAIL", then "cgwindow-test: N passed, M
 * failed"; the exit status is the number of failures. Needs the new
 * server up.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <SkyLight/SkyLight.h>
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

	/* Captured, the display shows what the app draws in its context. */
	check(CGDisplayCapture(main) == kCGErrorSuccess, "the display is captured");
	CGContextRef context = CGDisplayGetDrawingContext(main);
	check(context != NULL, "its drawing context");
	if (context != NULL) {
		CGContextSetRGBFillColor(context, 0, 1, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, 4000, 4000));
	}
	usleep(200000);	/* a refresh or two: the context is copied to the screen */
	image = CGDisplayCreateImageForRect(main, CGRectMake(10, 10, 20, 20));
	check(image != NULL && pixel(image, 5, 5) == 0xff00ff00, "what it draws reaches the screen");
	if (image != NULL)
		CGImageRelease(image);
	check(CGDisplayRelease(main) == kCGErrorSuccess, "and it is released");
	image = CGDisplayCreateImageForRect(main, CGRectMake(90, 110, 100, 80));
	check(image != NULL && pixel(image, 20, 20) == 0xffff0000, "the windows show again");
	if (image != NULL)
		CGImageRelease(image);

	SLSReleaseWindow(blue);
	SLSReleaseWindow(red);
	printf("cgwindow-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
