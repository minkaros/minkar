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
 * skylight-test: calls each function of the window server's interface
 * through SkyLight and checks the replies (WindowServer plan, WS1). One
 * line per check, "ok" or "FAIL", then "skylight-test: N passed, M
 * failed"; the exit status is the number of failures. Run it with the new
 * server up (SkyLight.framework/Resources/WindowServer).
 */

#include <math.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <SkyLight/SkyLight.h>

static int passed, failed;

static void
check(int good, const char *what, const char *detail)
{
	printf("%-4s %s%s%s\n", good ? "ok" : "FAIL", what, detail ? ": " : "",
	    detail ? detail : "");
	if (good)
		passed++;
	else
		failed++;
}

static void
testConnection(void)
{
	char detail[64];
	SLSConnectionID cid = SLSMainConnectionID();

	check(SLSServerPort() != MACH_PORT_NULL, "the server's service is found", NULL);
	snprintf(detail, sizeof(detail), "%d", cid);
	check(cid > 0, "a connection is opened", detail);
	check(SLSMainConnectionID() == cid, "the main connection is kept", NULL);
}

static int
inList(SLSDisplayID display, const SLSDisplayID *list, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++)
		if (list[i] == display)
			return 1;
	return 0;
}

static void
testDisplays(void)
{
	char detail[128];
	SLSDisplayID main = SLSMainDisplayID(), list[32];
	SLSDisplayInfo info;
	uint32_t n = 0;
	int err;

	snprintf(detail, sizeof(detail), "0x%x", main);
	check(main != 0, "the main display is known", detail);

	err = SLSGetOnlineDisplayList(32, list, &n);
	snprintf(detail, sizeof(detail), "%u displays", n);
	check(err == kSLSErrorSuccess && n > 0 && inList(main, list, n),
	    "the main display is online", detail);
	err = SLSGetActiveDisplayList(32, list, &n);
	check(err == kSLSErrorSuccess && n > 0 && inList(main, list, n),
	    "the main display is active", NULL);
	n = 0;
	err = SLSGetActiveDisplayList(0, NULL, &n);
	check(err == kSLSErrorSuccess && n > 0, "the display count alone", NULL);

	err = SLSGetDisplayInfo(main, &info);
	snprintf(detail, sizeof(detail), "%.0f,%.0f %.0fx%.0f", info.x, info.y,
	    info.width, info.height);
	check(err == kSLSErrorSuccess && info.display == main && info.x == 0 &&
	    info.y == 0 && info.width > 0 && info.height > 0,
	    "the main display's bounds start at the origin", detail);
	snprintf(detail, sizeof(detail), "0x%x", info.flags);
	check((info.flags & (kSLSDisplayOnline | kSLSDisplayActive | kSLSDisplayMain)) ==
	    (kSLSDisplayOnline | kSLSDisplayActive | kSLSDisplayMain),
	    "the main display is online, active and main", detail);
	snprintf(detail, sizeof(detail), "%.0fx%.0f mm", info.widthMM, info.heightMM);
	check(info.widthMM > 0 && info.heightMM > 0, "the physical size is known", detail);
	check(info.primary == main && info.mirrorOf == 0, "the main display is not mirrored",
	    NULL);

	n = 0;
	err = SLSGetDisplaysWithPoint(10, 10, 32, list, &n);
	check(err == kSLSErrorSuccess && n >= 1 && inList(main, list, n),
	    "a point near the origin is on the main display", NULL);
	n = 1;
	err = SLSGetDisplaysWithPoint(-10, -10, 32, list, &n);
	check(err == kSLSErrorSuccess && n == 0, "a point left of the origin is on none", NULL);
	n = 0;
	err = SLSGetDisplaysWithRect(-50, -50, 100, 100, 32, list, &n);
	check(err == kSLSErrorSuccess && n >= 1 && inList(main, list, n),
	    "a rect over the origin meets the main display", NULL);

	err = SLSGetDisplayInfo(0xdeadbeef, &info);
	check(err == kSLSErrorIllegalArgument, "an unknown display is refused", NULL);
	/* The menu bar (interface version 6). */
	{
		uint32_t h = 0;
		check(SLSGetDisplayMenuBarHeight(main, &h) == kSLSErrorSuccess &&
		    h == kSLSMenuBarHeight, "the main display has a menu bar", NULL);
		check(SLSGetDisplayMenuBarHeight(0xdeadbeef, &h) == kSLSErrorIllegalArgument,
		    "an unknown display has none", NULL);
	}
}

static void
testModes(void)
{
	char detail[128];
	SLSDisplayID main = SLSMainDisplayID();
	SLSDisplayMode modes[kSLSMaxDisplayModes], current;
	SLSDisplayInfo info, after;
	SLSDisplayConfigRef config;
	uint32_t n = 0, all = 0;
	int err, found = 0;

	err = SLSCopyDisplayModes(main, kSLSMaxDisplayModes, modes, &n);
	snprintf(detail, sizeof(detail), "%u modes", n);
	check(err == kSLSErrorSuccess && n > 0, "the main display lists its modes", detail);
	err = SLSCopyDisplayModes(main, 0, NULL, &all);
	check(err == kSLSErrorSuccess && all == n, "the mode count alone", NULL);

	err = SLSGetCurrentDisplayMode(main, &current);
	SLSGetDisplayInfo(main, &info);
	snprintf(detail, sizeof(detail), "id %u %ux%u %u bpp %.0f Hz", current.modeID,
	    current.pixelWidth, current.pixelHeight, current.depth, current.refreshRate);
	check(err == kSLSErrorSuccess && current.pixelWidth == info.width &&
	    current.pixelHeight == info.height && current.depth == 32,
	    "the current mode is the display's size, 32 bits per pixel", detail);
	for (uint32_t i = 0; i < n; i++)
		if (modes[i].modeID == current.modeID)
			found = 1;
	check(found, "the current mode is in the list", NULL);
	check(SLSGetCurrentDisplayMode(0xdeadbeef, &current) == kSLSErrorIllegalArgument &&
	    SLSCopyDisplayModes(0xdeadbeef, 0, NULL, &all) == kSLSErrorIllegalArgument,
	    "an unknown display has no modes", NULL);
	SLSGetCurrentDisplayMode(main, &current);

	/* Configurations. */
	err = SLSBeginDisplayConfiguration(&config);
	check(err == kSLSErrorSuccess && SLSCancelDisplayConfiguration(config) == kSLSErrorSuccess,
	    "a configuration is begun and cancelled", NULL);
	SLSBeginDisplayConfiguration(&config);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) == kSLSErrorSuccess,
	    "an empty configuration completes", NULL);

	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayWithDisplayMode(config, main, current.modeID);
	err = SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession);
	SLSGetDisplayInfo(main, &after);
	check(err == kSLSErrorSuccess && after.width == info.width && after.height == info.height,
	    "the current mode is set again", NULL);

	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayOrigin(config, main, 100, 50);
	err = SLSCompleteDisplayConfiguration(config, kSLSConfigureForAppOnly);
	SLSGetDisplayInfo(main, &after);
	snprintf(detail, sizeof(detail), "%.0f,%.0f", after.x, after.y);
	check(err == kSLSErrorSuccess && after.x == 0 && after.y == 0,
	    "the main display stays at the origin", detail);

	/* A bad change refuses the whole configuration. */
	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayOrigin(config, main, 10, 10);
	SLSConfigureDisplayWithDisplayMode(config, main, 0xdeadbeef);
	err = SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession);
	SLSGetCurrentDisplayMode(main, &modes[0]);
	check(err == kSLSErrorIllegalArgument && modes[0].modeID == current.modeID,
	    "an unknown mode refuses the configuration", NULL);
	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayOrigin(config, 0xdeadbeef, 0, 0);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) ==
	    kSLSErrorIllegalArgument, "a change to an unknown display is refused", NULL);
	SLSBeginDisplayConfiguration(&config);
	check(SLSCompleteDisplayConfiguration(config, 7) == kSLSErrorIllegalArgument,
	    "an unknown option is refused", NULL);

	SLSBeginDisplayConfiguration(&config);
	for (int i = 0; i < kSLSMaxDisplayConfigOps; i++)
		SLSConfigureDisplayOrigin(config, main, 0, 0);
	check(SLSConfigureDisplayOrigin(config, main, 0, 0) == kSLSErrorRangeCheck,
	    "a configuration holds at most 32 changes", NULL);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) == kSLSErrorSuccess,
	    "and 32 complete", NULL);
}

static int
near(float a, float b)
{
	return fabsf(a - b) < 0.002f;
}

static void
testGamma(void)
{
	char detail[128];
	SLSDisplayID main = SLSMainDisplayID();
	float r[kSLSGammaTableCapacity], g[kSLSGammaTableCapacity], b[kSLSGammaTableCapacity];
	float in[3] = { 0, 0.5f, 1 }, half[3] = { 0, 0.25f, 0.5f };
	SLSGammaFormula f, got;
	uint32_t capacity = 0, n = 0;
	int err;

	err = SLSGetGammaTableCapacity(main, &capacity);
	snprintf(detail, sizeof(detail), "%u", capacity);
	check(err == kSLSErrorSuccess && capacity == kSLSGammaTableCapacity,
	    "the gamma table capacity", detail);

	SLSRestoreDisplayTransfers();
	err = SLSGetDisplayTransferByTable(main, capacity, r, g, b, &n);
	check(err == kSLSErrorSuccess && n == capacity && near(r[0], 0) &&
	    near(g[n / 2], (float)(n / 2) / (n - 1)) && near(b[n - 1], 1),
	    "the default transfer is the identity", NULL);
	err = SLSGetDisplayTransferByFormula(main, &got);
	check(err == kSLSErrorSuccess && got.redMin == 0 && got.redMax == 1 &&
	    got.redGamma == 1 && got.blueGamma == 1, "and so is its formula", NULL);

	/* A formula: red at half brightness, green squared. */
	f = (SLSGammaFormula){ 0, 0.5f, 1, 0, 1, 2, 0.1f, 0.9f, 1 };
	err = SLSSetDisplayTransferByFormula(main, &f);
	check(err == kSLSErrorSuccess, "a formula is set", NULL);
	SLSGetDisplayTransferByTable(main, capacity, r, g, b, &n);
	snprintf(detail, sizeof(detail), "red %.3f green %.3f blue %.3f", r[n - 1],
	    g[(n - 1) / 2], b[0]);
	check(near(r[n - 1], 0.5f) && near(g[(n - 1) / 2], powf((float)((n - 1) / 2) / (n - 1), 2)) &&
	    near(b[0], 0.1f) && near(b[n - 1], 0.9f), "its table follows the formula", detail);
	SLSGetDisplayTransferByFormula(main, &got);
	check(memcmp(&got, &f, sizeof(f)) == 0, "the formula is read back", NULL);
	f.greenGamma = 0;
	check(SLSSetDisplayTransferByFormula(main, &f) == kSLSErrorIllegalArgument,
	    "a gamma of 0 is refused", NULL);
	f.greenGamma = 1;
	f.redMin = 0.8f;
	check(SLSSetDisplayTransferByFormula(main, &f) == kSLSErrorIllegalArgument,
	    "a minimum above the maximum is refused", NULL);

	/* A three-entry table, resampled. */
	err = SLSSetDisplayTransferByTable(main, 3, in, half, in);
	SLSGetDisplayTransferByTable(main, capacity, r, g, b, &n);
	snprintf(detail, sizeof(detail), "green %.3f..%.3f", g[0], g[n - 1]);
	check(err == kSLSErrorSuccess && near(g[n - 1], 0.5f) && near(r[n - 1], 1) &&
	    near(g[(n - 1) / 2], 0.25f * ((float)((n - 1) / 2) / ((n - 1) / 2.0f))),
	    "a short table is resampled to the capacity", detail);
	SLSGetDisplayTransferByFormula(main, &got);
	check(got.redGamma == 1 && got.redMax == 1, "a table resets the formula", NULL);
	in[1] = NAN;
	check(SLSSetDisplayTransferByTable(main, 3, in, half, in) == kSLSErrorIllegalArgument,
	    "a table with a NaN is refused", NULL);
	check(SLSSetDisplayTransferByTable(main, 1, in, half, in) == kSLSErrorIllegalArgument,
	    "a one-entry table is refused", NULL);
	check(SLSSetDisplayTransferByTable(0xdeadbeef, 3, half, half, half) ==
	    kSLSErrorIllegalArgument, "an unknown display has no transfer", NULL);

	check(SLSRestoreDisplayTransfers() == kSLSErrorSuccess, "the transfers are restored", NULL);
	SLSGetDisplayTransferByTable(main, capacity, r, g, b, &n);
	check(near(r[n - 1], 1) && near(g[n - 1], 1), "to the identity", NULL);
	n = 99;
	check(SLSGetDisplayTransferByTable(main, 0, NULL, NULL, NULL, &n) == kSLSErrorSuccess &&
	    n == 0, "a table read with no room returns nothing", NULL);
}

static void
testMirrorAndFade(void)
{
	char detail[96];
	SLSDisplayID main = SLSMainDisplayID();
	SLSDisplayConfigRef config;
	SLSFadeReservation r = 0, other = 0;
	SLSDisplayInfo info;
	float blend = -1;
	int32_t holder = -1;

	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayMirrorOfDisplay(config, main, main);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) ==
	    kSLSErrorIllegalArgument, "a display does not mirror itself", NULL);
	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayMirrorOfDisplay(config, main, 0xdeadbeef);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) ==
	    kSLSErrorIllegalArgument, "nor an unknown display", NULL);
	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayMirrorOfDisplay(config, main, 0);
	check(SLSCompleteDisplayConfiguration(config, kSLSConfigureForSession) == kSLSErrorSuccess &&
	    SLSGetDisplayInfo(main, &info) == kSLSErrorSuccess && info.mirrorOf == 0 &&
	    !(info.flags & kSLSDisplayMirrored), "mirroring is turned off", NULL);

	SLSGetFadeState(&blend, &holder);
	check(blend == 0 && holder == 0, "no fade and no reservation", NULL);
	check(SLSAcquireFadeReservation(20, &r) == kSLSErrorIllegalArgument &&
	    SLSAcquireFadeReservation(0, &r) == kSLSErrorIllegalArgument,
	    "a reservation is 15 seconds at most, and more than 0", NULL);
	check(SLSAcquireFadeReservation(5, &r) == kSLSErrorSuccess && r != 0,
	    "a fade reservation is acquired", NULL);
	SLSGetFadeState(&blend, &holder);
	check(holder == getpid(), "held by this process", NULL);
	check(SLSAcquireFadeReservation(5, &other) == kSLSErrorNoneAvailable,
	    "a second one is not available", NULL);
	check(SLSFadeDisplays(r, 0.2, 0, 1, 0, 0, 0, 1) == kSLSErrorSuccess,
	    "the displays fade to black, synchronously", NULL);
	SLSGetFadeState(&blend, &holder);
	snprintf(detail, sizeof(detail), "blend %.2f", blend);
	check(blend == 1, "the blend is the end blend", detail);
	check(SLSFadeDisplays(r, 0, 1, 2, 0, 0, 0, 0) == kSLSErrorIllegalArgument &&
	    SLSFadeDisplays(r, 0, 0, 1, NAN, 0, 0, 0) == kSLSErrorIllegalArgument,
	    "a blend or color outside 0..1 is refused", NULL);
	check(SLSFadeDisplays(r + 1, 0, 1, 0, 0, 0, 0, 0) == kSLSErrorIllegalArgument,
	    "another reservation number is refused", NULL);
	SLSFadeDisplays(r, 0, 1, 0, 0, 0, 0, 0);
	check(SLSReleaseFadeReservation(r) == kSLSErrorSuccess &&
	    SLSReleaseFadeReservation(r) == kSLSErrorIllegalArgument,
	    "the reservation is released, once", NULL);
	check(SLSFadeDisplays(r, 0, 0, 1, 0, 0, 0, 0) == kSLSErrorIllegalArgument,
	    "a released reservation fades nothing", NULL);
	check(SLSAcquireFadeReservation(0.3, &r) == kSLSErrorSuccess, "a short reservation", NULL);
	usleep(500000);
	SLSGetFadeState(&blend, &holder);
	check(holder == 0 && SLSFadeDisplays(r, 0, 0, 1, 0, 0, 0, 0) == kSLSErrorIllegalArgument,
	    "runs out", NULL);
}

static void
testCapture(void)
{
	char detail[128];
	SLSDisplayID main = SLSMainDisplayID();
	SLSDisplayInfo d;
	SLSWindowInfo w;
	int32_t pid = -1;
	uint32_t shield;
	int err;

	SLSGetDisplayInfo(main, &d);
	err = SLSDisplayCapturedBy(main, &pid);
	check(err == kSLSErrorSuccess && pid == 0, "the main display is not captured", NULL);
	check(SLSCaptureDisplay(main, 0) == kSLSErrorSuccess, "it is captured", NULL);
	SLSDisplayCapturedBy(main, &pid);
	snprintf(detail, sizeof(detail), "pid %d", pid);
	check(pid == getpid(), "by this process", detail);
	check(SLSCaptureDisplay(main, 0) == kSLSErrorSuccess, "capturing it again does nothing",
	    NULL);
	shield = SLSShieldingWindowID(main);
	err = SLSGetWindowInfo(shield, &w);
	snprintf(detail, sizeof(detail), "window %u level %d %.0fx%.0f", shield, w.level,
	    w.width, w.height);
	check(shield != 0 && err == kSLSErrorSuccess && w.level == kSLSShieldingWindowLevel &&
	    w.x == d.x && w.y == d.y && w.width == d.width && w.height == d.height &&
	    (w.flags & kSLSWindowOnScreen) && w.ownerPID == getpid(),
	    "a shielding window covers it, on screen", detail);
	check(SLSReleaseWindow(shield) == kSLSErrorCannotComplete,
	    "the shielding window is not released alone", NULL);
	check(SLSReleaseDisplay(main) == kSLSErrorSuccess, "the display is released", NULL);
	SLSDisplayCapturedBy(main, &pid);
	check(pid == 0 && SLSShieldingWindowID(main) == 0 &&
	    SLSGetWindowInfo(shield, &w) == kSLSErrorIllegalArgument,
	    "and its shielding window is gone", NULL);
	check(SLSCaptureDisplay(0, 0) == kSLSErrorSuccess && SLSShieldingWindowID(main) != 0,
	    "every display is captured", NULL);
	check(SLSReleaseDisplay(0) == kSLSErrorSuccess && SLSShieldingWindowID(main) == 0,
	    "and released", NULL);
	check(SLSCaptureDisplay(0xdeadbeef, 0) == kSLSErrorIllegalArgument,
	    "an unknown display is not captured", NULL);
}

static void
testCursor(void)
{
	char detail[128];
	SLSDisplayInfo d;
	SLSPoint p;
	int visible = -1;
	int32_t dx = 1, dy = 1;

	SLSGetDisplayInfo(SLSMainDisplayID(), &d);
	SLSCursorIsVisible(&visible);
	check(visible == 1, "the cursor is visible", NULL);
	SLSHideCursor(0);
	SLSCursorIsVisible(&visible);
	check(visible == 0, "hidden", NULL);
	SLSHideCursor(0);
	SLSShowCursor(0);
	SLSCursorIsVisible(&visible);
	check(visible == 0, "two hides need two shows", NULL);
	SLSShowCursor(0);
	SLSCursorIsVisible(&visible);
	check(visible == 1, "and then it shows", NULL);
	SLSShowCursor(0);
	SLSHideCursor(0);
	SLSCursorIsVisible(&visible);
	check(visible == 0, "an extra show is not kept for later", NULL);
	SLSShowCursor(0);

	check(SLSWarpCursorPosition((SLSPoint){ 100, 120 }) == kSLSErrorSuccess &&
	    SLSGetCursorPosition(&p) == kSLSErrorSuccess && p.x == 100 && p.y == 120,
	    "the cursor is warped", NULL);
	SLSWarpCursorPosition((SLSPoint){ -50, 99999 });
	SLSGetCursorPosition(&p);
	snprintf(detail, sizeof(detail), "%.0f,%.0f", p.x, p.y);
	check(p.x == 0 && p.y == d.height - 1, "a warp is kept on the displays", detail);
	check(SLSWarpCursorPosition((SLSPoint){ NAN, 0 }) == kSLSErrorIllegalArgument,
	    "a warp to NaN is refused", NULL);
	check(SLSGetLastMouseDelta(&dx, &dy) == kSLSErrorSuccess && dx == 0 && dy == 0,
	    "no mouse delta after a warp", NULL);
	check(SLSAssociateMouseAndCursorPosition(0) == kSLSErrorSuccess &&
	    SLSAssociateMouseAndCursorPosition(1) == kSLSErrorSuccess,
	    "the mouse is dissociated and associated again", NULL);
}

/* This process's windows in the list, front to back. */
static uint32_t
ownList(uint32_t option, SLSWindowID relative, SLSWindowID *own, uint32_t max)
{
	static SLSWindowID list[kSLSMaxWindowList];
	uint32_t n = 0, k = 0;

	SLSWindowListCopy(option, relative, kSLSMaxWindowList, list, &n);
	for (uint32_t i = 0; i < n && k < max; i++) {
		SLSWindowInfo info;

		if (SLSGetWindowInfo(list[i], &info) == kSLSErrorSuccess &&
		    info.ownerPID == getpid())
			own[k++] = list[i];
	}
	return k;
}

/* Status items (WS4a): windows made status items, laid out right to left
 * in the main display's menu bar. */
static double
windowX(SLSWindowID w, double *y)
{
	SLSWindowInfo info;

	memset(&info, 0, sizeof(info));
	SLSGetWindowInfo(w, &info);
	if (y != NULL)
		*y = info.y;
	return info.x;
}

static void
testStatusArea(void)
{
	SLSDisplayInfo d;
	SLSWindowID one = 0, two = 0;
	double y = -1, right;

	SLSGetDisplayInfo(SLSMainDisplayID(), &d);
	right = d.x + d.width;
	SLSNewWindow(kSLSStatusWindowLevel, (SLSRect){ 5, 300, 30, 22 }, &one);
	SLSNewWindow(kSLSStatusWindowLevel, (SLSRect){ 5, 300, 50, 22 }, &two);
	SLSOrderWindow(one, kSLSOrderAbove, 0);
	SLSOrderWindow(two, kSLSOrderAbove, 0);
	check(windowX(one, NULL) == 5, "a window at the status level stays where it is", NULL);
	check(SLSSetWindowStatusItem(one, 1) == kSLSErrorSuccess &&
	    SLSSetWindowStatusItem(two, 1) == kSLSErrorSuccess, "two windows become status items",
	    NULL);
	check(windowX(one, &y) == right - 30 && y == d.y,
	    "the first status item goes at the right of the menu bar", NULL);
	check(windowX(two, NULL) == right - 80, "the next one to its left", NULL);
	SLSSetWindowFrame(two, (SLSRect){ 0, 0, 40, 22 });
	check(windowX(two, &y) == right - 70 && y == d.y,
	    "a status item keeps its width and the server its place", NULL);
	SLSOrderWindow(one, kSLSOrderOut, 0);
	check(windowX(two, NULL) == right - 40, "the items close up when one goes", NULL);
	check(SLSSetWindowStatusItem(two, 0) == kSLSErrorSuccess, "a status item stops being one",
	    NULL);
	SLSSetWindowFrame(two, (SLSRect){ 5, 300, 40, 22 });
	check(windowX(two, NULL) == 5, "and its window is its own again", NULL);
	check(SLSSetWindowStatusItem(0xdeadbeef, 1) == kSLSErrorIllegalArgument,
	    "an unknown window is no status item", NULL);
	/* Its app was told where the server put each one; the queue is read
	 * empty for the next tests. */
	{
		SLSEvent events[kSLSMaxEvents];
		uint32_t n = 0;
		int told = 0;
		while (SLSReceiveEvents(events, kSLSMaxEvents, &n, 0) == kSLSErrorSuccess && n > 0)
			for (uint32_t i = 0; i < n; i++)
				if (events[i].type == kSLSEventWindowMoved && events[i].window == one &&
				    events[i].x == right - 30)
					told = 1;
		check(told, "the app is told where its status item went", NULL);
	}
	SLSReleaseWindow(one);
	SLSReleaseWindow(two);
}

static void
testWindows(void)
{
	char detail[160];
	SLSWindowID a = 0, b = 0, desk = 0, l[8];
	SLSWindowInfo info;
	SLSRect frame = { 10, 20, 300, 200 };
	uint32_t n;
	int err;

	err = SLSNewWindow(0, frame, &a);
	check(err == kSLSErrorSuccess && a != 0, "a window is made", NULL);
	err = SLSGetWindowInfo(a, &info);
	snprintf(detail, sizeof(detail), "%s, level %d, %.0f,%.0f %.0fx%.0f", info.ownerName,
	    info.level, info.x, info.y, info.width, info.height);
	check(err == kSLSErrorSuccess && info.window == a && info.ownerPID == getpid() &&
	    info.connection == SLSMainConnectionID() && info.level == 0 && info.x == 10 &&
	    info.width == 300 && info.alpha == 1 && strcmp(info.ownerName, "skylight-test") == 0,
	    "its info names its owner and frame", detail);
	check(!(info.flags & kSLSWindowOnScreen) && ownList(kSLSWindowListOnScreenOnly, 0, l, 8) == 0 &&
	    ownList(kSLSWindowListAll, 0, l, 8) == 1, "it starts ordered out, listed with all", NULL);

	SLSNewWindow(0, frame, &b);
	SLSOrderWindow(a, kSLSOrderAbove, 0);
	SLSOrderWindow(b, kSLSOrderAbove, 0);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 2 && l[0] == b && l[1] == a, "ordered in, the last is in front", NULL);
	SLSOrderWindow(b, kSLSOrderBelow, a);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 2 && l[0] == a && l[1] == b, "ordered below another", NULL);
	SLSOrderWindow(b, kSLSOrderAbove, a);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 2 && l[0] == b && l[1] == a, "and above it again", NULL);
	SLSSetWindowLevel(a, 3);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 2 && l[0] == a && l[1] == b, "a higher level comes first", NULL);
	SLSOrderWindow(b, kSLSOrderAbove, 0);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 2 && l[0] == a, "and a lower one cannot pass it", NULL);

	n = ownList(kSLSWindowListOnScreenBelowWindow, a, l, 8);
	check(n == 1 && l[0] == b, "the windows below one", NULL);
	n = ownList(kSLSWindowListOnScreenAboveWindow | kSLSWindowListIncludingWindow, b, l, 8);
	check(n == 2 && l[0] == a && l[1] == b, "the windows above one, with it", NULL);
	n = ownList(kSLSWindowListIncludingWindow, b, l, 8);
	check(n == 1 && l[0] == b, "only one window", NULL);
	check(ownList(kSLSWindowListOnScreenAboveWindow, 0xdeadbeef, l, 8) == 0,
	    "relative to an unknown window, none", NULL);

	SLSNewWindow(kSLSDesktopIconWindowLevel, frame, &desk);
	SLSOrderWindow(desk, kSLSOrderAbove, 0);
	n = ownList(kSLSWindowListOnScreenOnly, 0, l, 8);
	check(n == 3 && l[2] == desk, "a desktop window is at the back", NULL);
	n = ownList(kSLSWindowListOnScreenOnly | kSLSWindowListExcludeDesktopElements, 0, l, 8);
	check(n == 2, "and left out with the desktop elements", NULL);

	check(SLSSetWindowFrame(b, (SLSRect){ 5, 6, 70, 80 }) == kSLSErrorSuccess &&
	    SLSGetWindowInfo(b, &info) == kSLSErrorSuccess && info.x == 5 && info.height == 80,
	    "a window moves", NULL);
	check(SLSSetWindowFrame(b, (SLSRect){ 0, 0, -1, 10 }) == kSLSErrorIllegalArgument &&
	    SLSNewWindow(0, (SLSRect){ 0, 0, NAN, 1 }, &l[0]) == kSLSErrorIllegalArgument,
	    "a negative or NaN size is refused", NULL);
	check(SLSOrderWindow(b, kSLSOrderAbove, b) == kSLSErrorIllegalArgument &&
	    SLSOrderWindow(b, 7, 0) == kSLSErrorIllegalArgument,
	    "ordering relative to itself or with a bad place is refused", NULL);
	SLSOrderWindow(b, kSLSOrderOut, 0);
	SLSGetWindowInfo(b, &info);
	check(!(info.flags & kSLSWindowOnScreen) && ownList(kSLSWindowListOnScreenOnly, 0, l, 8) == 2,
	    "a window is ordered out", NULL);
	check(SLSReleaseWindow(a) == kSLSErrorSuccess && SLSReleaseWindow(b) == kSLSErrorSuccess &&
	    SLSReleaseWindow(desk) == kSLSErrorSuccess && ownList(kSLSWindowListAll, 0, l, 8) == 0,
	    "the windows are released", NULL);
	check(SLSReleaseWindow(a) == kSLSErrorIllegalArgument &&
	    SLSGetWindowInfo(a, &info) == kSLSErrorIllegalArgument,
	    "a released window is unknown", NULL);
}

/*
 * The child of testClientExit: it captures the main display, makes a
 * window, hides the cursor, changes the transfer table and a display for
 * itself only, says "ready" and waits for its standard input to close.
 */
static int
childMain(void)
{
	SLSDisplayID main = SLSMainDisplayID();
	SLSGammaFormula f = { 0, 0.5f, 1, 0, 0.5f, 1, 0, 0.5f, 1 };
	SLSDisplayConfigRef config;
	SLSDisplayMode mode;
	SLSFadeReservation r = 0;
	SLSWindowID w = 0;
	char c;

	if (SLSCaptureDisplay(main, 0) != kSLSErrorSuccess ||
	    SLSNewWindow(0, (SLSRect){ 0, 0, 50, 50 }, &w) != kSLSErrorSuccess ||
	    SLSOrderWindow(w, kSLSOrderAbove, 0) != kSLSErrorSuccess ||
	    SLSHideCursor(0) != kSLSErrorSuccess ||
	    SLSSetDisplayTransferByFormula(main, &f) != kSLSErrorSuccess ||
	    SLSGetCurrentDisplayMode(main, &mode) != kSLSErrorSuccess ||
	    SLSAcquireFadeReservation(10, &r) != kSLSErrorSuccess ||
	    SLSFadeDisplays(r, 0, 0, 0.5f, 0, 0, 0, 0) != kSLSErrorSuccess)
		return 1;
	SLSBeginDisplayConfiguration(&config);
	SLSConfigureDisplayWithDisplayMode(config, main, mode.modeID);
	if (SLSCompleteDisplayConfiguration(config, kSLSConfigureForAppOnly) != kSLSErrorSuccess)
		return 1;
	printf("ready %u\n", w);
	fflush(stdout);
	while (read(0, &c, 1) > 0)
		;
	return 0;
}

static void
testClientExit(const char *self)
{
	char detail[128], line[64];
	int toChild[2], fromChild[2], status = 0, visible = -1;
	SLSDisplayID main = SLSMainDisplayID();
	SLSWindowInfo info;
	SLSGammaFormula got;
	posix_spawn_file_actions_t actions;
	char *argv[] = { (char *)self, "child", NULL };
	unsigned window = 0;
	int32_t pid = 0;
	pid_t child;
	FILE *in;

	if (pipe(toChild) != 0 || pipe(fromChild) != 0) {
		check(0, "a child client is started", "no pipe");
		return;
	}
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
	posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
	posix_spawn_file_actions_addclose(&actions, toChild[1]);
	posix_spawn_file_actions_addclose(&actions, fromChild[0]);
	if (posix_spawn(&child, self, &actions, NULL, argv, NULL) != 0) {
		check(0, "a child client is started", self);
		return;
	}
	close(toChild[0]);
	close(fromChild[1]);
	in = fdopen(fromChild[0], "r");
	if (in == NULL || fgets(line, sizeof(line), in) == NULL ||
	    sscanf(line, "ready %u", &window) != 1) {
		check(0, "a child client is started", "it did not get ready");
		close(toChild[1]);
		waitpid(child, &status, 0);
		return;
	}
	check(1, "a child client is started", NULL);

	SLSDisplayCapturedBy(main, &pid);
	check(pid == child, "the child captured the main display", NULL);
	check(SLSCaptureDisplay(main, 0) == kSLSErrorCannotComplete &&
	    SLSReleaseDisplay(main) == kSLSErrorCannotComplete,
	    "this process can neither capture nor release it", NULL);
	check(SLSGetWindowInfo(window, &info) == kSLSErrorSuccess && info.ownerPID == child,
	    "the child's window is listed as its", NULL);
	check(SLSReleaseWindow(window) == kSLSErrorCannotComplete &&
	    SLSOrderWindow(window, kSLSOrderOut, 0) == kSLSErrorCannotComplete,
	    "this process cannot change the child's window", NULL);
	check(SLSWindowSetDragRegion(window, NULL, 0, NULL, 0) == kSLSErrorCannotComplete,
	    "nor its drag region", NULL);
	SLSCursorIsVisible(&visible);
	check(visible == 0, "the child hid the cursor", NULL);
	{
		float blend = 0;
		int32_t holder = 0;

		SLSGetFadeState(&blend, &holder);
		check(holder == child && blend == 0.5f, "and holds a fade reservation, half faded",
		    NULL);
	}

	close(toChild[1]);
	waitpid(child, &status, 0);
	fclose(in);
	/* The server hears of the exit asynchronously. */
	for (int i = 0; i < 40; i++) {
		SLSDisplayCapturedBy(main, &pid);
		if (pid == 0)
			break;
		usleep(50000);
	}
	snprintf(detail, sizeof(detail), "exit status %d", WEXITSTATUS(status));
	check(pid == 0 && SLSShieldingWindowID(main) == 0,
	    "when the child exits, its capture is released", detail);
	check(SLSGetWindowInfo(window, &info) == kSLSErrorIllegalArgument,
	    "its window is released", NULL);
	SLSCursorIsVisible(&visible);
	check(visible == 1, "the cursor shows again", NULL);
	SLSGetDisplayTransferByFormula(main, &got);
	check(got.redMax == 1 && got.greenMax == 1, "its transfer table is undone", NULL);
	{
		float blend = 1;
		int32_t holder = 1;

		SLSGetFadeState(&blend, &holder);
		check(holder == 0 && blend == 0, "its reservation and fade end", NULL);
	}
	check(SLSCaptureDisplay(main, 0) == kSLSErrorSuccess &&
	    SLSReleaseDisplay(main) == kSLSErrorSuccess, "and the display can be captured again",
	    NULL);
}

static void
fill(uint8_t *pixels, uint32_t rowBytes, SLSIntRect r, uint32_t color)
{
	for (int32_t y = r.y; y < r.y + r.height; y++)
		for (int32_t x = r.x; x < r.x + r.width; x++)
			((uint32_t *)(pixels + (size_t)y * rowBytes))[x] = color;
}

static SLSServerStats
statsAfterFrames(void)
{
	SLSServerStats st;

	usleep(150000);		/* a few display link periods */
	SLSGetServerStats(&st);
	return st;
}

static void
testSurfaces(void)
{
	char detail[128];
	SLSWindowID w = 0, empty = 0;
	SLSWindowInfo info;
	SLSServerStats before, after, idle;
	void *pixels = NULL, *again = NULL;
	uint32_t width = 0, height = 0, rowBytes = 0, color = 0;
	int err;

	SLSNewWindow(0, (SLSRect){ 0, 0, 0, 0 }, &empty);
	check(SLSWindowBackBuffer(empty, &pixels, &width, &height, &rowBytes) ==
	    kSLSErrorIllegalArgument, "a window of no size has no surface", NULL);
	SLSReleaseWindow(empty);

	SLSNewWindow(0, (SLSRect){ 40, 40, 64, 32 }, &w);
	SLSOrderWindow(w, kSLSOrderAbove, 0);
	err = SLSWindowBackBuffer(w, &pixels, &width, &height, &rowBytes);
	snprintf(detail, sizeof(detail), "%ux%u, %u bytes a row", width, height, rowBytes);
	check(err == kSLSErrorSuccess && pixels != NULL && width == 64 && height == 32 &&
	    rowBytes >= 256, "a window gets a surface of its size", detail);
	check(((uint32_t *)pixels)[0] == 0 && ((uint32_t *)pixels)[64 * 32 - 1] == 0,
	    "it starts transparent", NULL);

	before = statsAfterFrames();
	fill(pixels, rowBytes, (SLSIntRect){ 0, 0, 64, 32 }, 0xffff0000);
	err = SLSWindowCommit(w, (SLSIntRect[]){ { 0, 0, 64, 32 } }, 1);
	after = statsAfterFrames();
	snprintf(detail, sizeof(detail), "%llu frame(s), %llu pixels",
	    (unsigned long long)(after.frames - before.frames),
	    (unsigned long long)after.lastDamagedPixels);
	check(err == kSLSErrorSuccess && after.commits == before.commits + 1 &&
	    after.frames > before.frames && after.lastDamagedPixels >= 64 * 32,
	    "a commit is composited", detail);

	SLSWindowBackBuffer(w, &again, &width, &height, &rowBytes);
	check(again != pixels && ((uint32_t *)again)[0] == 0xffff0000 &&
	    ((uint32_t *)again)[64 * 32 - 1] == 0xffff0000,
	    "the new back buffer holds what was committed", NULL);

	idle = statsAfterFrames();
	usleep(300000);
	SLSGetServerStats(&after);
	snprintf(detail, sizeof(detail), "%llu idle ticks",
	    (unsigned long long)(after.idleTicks - idle.idleTicks));
	check(after.frames == idle.frames && after.idleTicks > idle.idleTicks,
	    "no frame while nothing changes", detail);

	fill(again, rowBytes, (SLSIntRect){ 8, 8, 4, 4 }, 0xff00ff00);
	before = after;
	err = SLSWindowCommit(w, (SLSIntRect[]){ { 8, 8, 4, 4 } }, 1);
	after = statsAfterFrames();
	snprintf(detail, sizeof(detail), "%llu pixels", (unsigned long long)after.lastDamagedPixels);
	check(err == kSLSErrorSuccess && after.frames == before.frames + 1 &&
	    after.lastDamagedPixels == 16, "only the damaged area is composited", detail);

	color = 0xff0000ff;
	{
		static uint32_t own[64 * 32];
		for (int i = 0; i < 64 * 32; i++)
			own[i] = color;
		err = SLSWindowFlushPixels(w, own, 64 * 4, (SLSIntRect[]){ { 0, 0, 64, 32 } }, 1);
		SLSWindowBackBuffer(w, &again, &width, &height, &rowBytes);
		check(err == kSLSErrorSuccess && ((uint32_t *)again)[100] == color,
		    "pixels are flushed from the client's own buffer", NULL);
	}

	SLSSetWindowFrame(w, (SLSRect){ 40, 40, 80, 40 });
	err = SLSWindowBackBuffer(w, &again, &width, &height, &rowBytes);
	snprintf(detail, sizeof(detail), "%ux%u", width, height);
	check(err == kSLSErrorSuccess && width == 80 && height == 40,
	    "a new size gets a new surface", detail);
	SLSSetWindowFrame(w, (SLSRect){ 60, 50, 80, 40 });
	SLSWindowBackBuffer(w, &pixels, &width, &height, &rowBytes);
	check(pixels == again, "a move keeps the surface", NULL);

	check(SLSSetWindowAlpha(w, 0.5) == kSLSErrorSuccess &&
	    SLSGetWindowInfo(w, &info) == kSLSErrorSuccess && info.alpha == 0.5,
	    "a window's alpha is set", NULL);
	check(SLSSetWindowAlpha(w, 1.5) == kSLSErrorIllegalArgument &&
	    SLSSetWindowAlpha(w, NAN) == kSLSErrorIllegalArgument,
	    "an alpha outside 0..1 is refused", NULL);
	check(SLSWindowCommit(0xdeadbeef, NULL, 0) == kSLSErrorIllegalArgument,
	    "a commit to an unknown window is refused", NULL);
	SLSReleaseWindow(w);
}

static SLSWindowID solidWindow(SLSRect frame, uint32_t color);
#define solidWindowAt solidWindow

static void
testEvents(void)
{
	SLSEvent events[kSLSMaxEvents];
	SLSWindowID w = 0;
	uint32_t n = 99;

	check(SLSEventPort() != MACH_PORT_NULL, "the connection has an event port", NULL);
	check(SLSReceiveEvents(events, kSLSMaxEvents, &n, 0) == kSLSErrorSuccess && n == 0,
	    "no event is waiting", NULL);
	SLSNewWindow(0, (SLSRect){ 10, 10, 20, 20 }, &w);
	check(SLSSetKeyWindow(w) == kSLSErrorSuccess && SLSGetKeyWindow() == w,
	    "a window is made key", NULL);
	check(SLSSetKeyWindow(0xdeadbeef) == kSLSErrorIllegalArgument,
	    "an unknown window is not made key", NULL);
	check(SLSWindowStartMove(w) == kSLSErrorCannotComplete,
	    "a move needs the left button down", NULL);
	/* Drag regions (interface version 3). */
	SLSIntRect title = { 0, 0, 20, 5 }, button = { 1, 1, 3, 3 };
	check(SLSWindowSetDragRegion(w, &title, 1, &button, 1) == kSLSErrorSuccess,
	    "a drag region is set", NULL);
	check(SLSWindowSetDragRegion(w, NULL, 0, NULL, 0) == kSLSErrorSuccess,
	    "and taken away", NULL);
	check(SLSWindowSetDragRegion(w, &title, kSLSMaxRegionRects + 1, NULL, 0) ==
	    kSLSErrorIllegalArgument, "too many drag rectangles are refused", NULL);
	check(SLSWindowSetDragRegion(0xdeadbeef, &title, 1, NULL, 0) == kSLSErrorIllegalArgument,
	    "an unknown window gets no drag region", NULL);
	/* Symbolic hotkeys (interface version 4). */
	{
		uint16_t code = 0;
		uint32_t mods = 0;
		int on = 0;
		check(SLSGetSymbolicHotKey(kSLSHotKeyAppSwitcher, &code, &mods, &on) ==
		    kSLSErrorSuccess && code == 48 && mods == (1u << 20) && on,
		    "the app switcher hotkey is Command-Tab", NULL);
		check(SLSGetSymbolicHotKey(kSLSHotKeyMenuBarFocus, &code, &mods, &on) ==
		    kSLSErrorSuccess && code == 120 && mods == (1u << 18) && on,
		    "the menu bar focus hotkey is Control-F2", NULL);
		check(SLSGetSymbolicHotKey(99, &code, &mods, &on) == kSLSErrorIllegalArgument,
		    "an unknown hotkey is refused", NULL);
		check(SLSClaimSymbolicHotKey(kSLSHotKeyMenuBarFocus, 1) == kSLSErrorIllegalArgument,
		    "the front app's hotkey cannot be claimed", NULL);
		check(SLSClaimSymbolicHotKey(kSLSHotKeyForceQuit, 1) == kSLSErrorSuccess &&
		    SLSClaimSymbolicHotKey(kSLSHotKeyForceQuit, 0) == kSLSErrorSuccess,
		    "a hotkey is claimed and given back", NULL);
	}
	/* The front app (interface version 5): this process, made so by its
	 * key window; asked for, it stays in front with no window. */
	{
		pid_t front = 0;
		check(SLSGetFrontProcess(&front) == kSLSErrorSuccess && front == getpid(),
		    "a key window's owner is the front app", NULL);
		SLSReleaseWindow(w);
		check(SLSGetKeyWindow() == 0, "a released key window is key no more", NULL);
		check(SLSGetFrontProcess(&front) == kSLSErrorSuccess && front == getpid(),
		    "and its app stays in front", NULL);
		check(SLSSetFrontProcess() == kSLSErrorSuccess &&
		    SLSGetFrontProcess(&front) == kSLSErrorSuccess && front == getpid(),
		    "an app with no window is made front", NULL);
		check(SLSGetFrontProcess(NULL) == kSLSErrorIllegalArgument,
		    "the front process needs somewhere to go", NULL);
	}
	/* Pointer capture (interface version 7). */
	{
		SLSWindowID menu = 0;
		SLSNewWindow(0, (SLSRect){ 40, 40, 20, 20 }, &menu);
		check(SLSCapturePointer(menu) == kSLSErrorSuccess && SLSGetPointerCapture() == menu,
		    "the front app captures the pointer", NULL);
		check(SLSCapturePointer(0) == kSLSErrorSuccess && SLSGetPointerCapture() == 0,
		    "and releases it", NULL);
		check(SLSCapturePointer(0xdeadbeef) == kSLSErrorIllegalArgument,
		    "an unknown window cannot capture it", NULL);
		SLSCapturePointer(menu);
		SLSReleaseWindow(menu);
		check(SLSGetPointerCapture() == 0, "a released window holds it no more", NULL);
	}
}

/*
 * skylight-test events: a white window at 100,100 (400 x 300), key, and
 * every event it gets printed, one line each, until it is killed. A mouse
 * down in its top 20 pixels starts a server-side move. With "grab" the
 * window captures the pointer first, as an open menu does. For the local
 * input tests.
 */
static int
eventsMain(int grab)
{
	SLSWindowID w = solidWindowAt((SLSRect){ 100, 100, 400, 300 }, 0xffffffff);
	SLSEvent events[kSLSMaxEvents];

	SLSSetKeyWindow(w);
	/* "events grab": the window holds the pointer, as an open menu. */
	if (grab)
		printf("grab %d\n", SLSCapturePointer(w));
	printf("ready %u\n", w);
	fflush(stdout);
	for (;;) {
		uint32_t n = 0;

		if (SLSReceiveEvents(events, kSLSMaxEvents, &n, 1) != kSLSErrorSuccess)
			return 1;
		for (uint32_t i = 0; i < n; i++) {
			SLSEvent *e = &events[i];
			if (e->type == kSLSEventLeftMouseDown) {
				SLSWindowInfo info;
				SLSGetWindowInfo(w, &info);
				if (e->y - info.y < 20)
					printf("move %d\n", SLSWindowStartMove(w));
			}
			printf("event type=%u window=%u x=%.0f y=%.0f click=%d button=%u key=%u "
			    "chars=%s repeat=%u flags=0x%x\n", e->type, e->window, e->x, e->y,
			    e->clickCount, e->buttonNumber, e->keyCode, e->characters, e->isRepeat,
			    e->flags);
		}
		fflush(stdout);
	}
}

/*
 * skylight-test hotkeys: claims the app switcher and force quit hotkeys
 * (what the Dock and loginwindow do) and prints the ones it gets, for the
 * local input tests: "hotkey N key=K flags=0x..".
 */
static int
hotkeysMain(void)
{
	SLSEvent events[kSLSMaxEvents];

	if (SLSClaimSymbolicHotKey(kSLSHotKeyAppSwitcher, 1) != kSLSErrorSuccess ||
	    SLSClaimSymbolicHotKey(kSLSHotKeyAppSwitcherBackward, 1) != kSLSErrorSuccess ||
	    SLSClaimSymbolicHotKey(kSLSHotKeyForceQuit, 1) != kSLSErrorSuccess)
		return 1;
	printf("claimed\n");
	fflush(stdout);
	for (;;) {
		uint32_t n = 0;

		if (SLSReceiveEvents(events, kSLSMaxEvents, &n, 1) != kSLSErrorSuccess)
			return 1;
		for (uint32_t i = 0; i < n; i++)
			if (events[i].type == kSLSEventSymbolicHotKey)
				printf("hotkey %u key=%u flags=0x%x\n", events[i].buttonNumber,
				    events[i].keyCode, events[i].flags);
		fflush(stdout);
	}
}

/*
 * skylight-test draw: windows for the screen checks of the local tests.
 * A red window at 100,100 (300 x 200), a blue one over it at 250,200 and a
 * green one at 600,100 (100 x 100) at half alpha. Then commands, one at a
 * time, from /tmp/ws-draw.cmd (deleted when done): "raise-red", "move-blue
 * X Y", "cursor X Y" (a warp), "quit".
 */
static SLSWindowID
solidWindow(SLSRect frame, uint32_t color)
{
	SLSWindowID w = 0;
	void *pixels;
	uint32_t width, height, rowBytes;

	SLSNewWindow(0, frame, &w);
	SLSWindowBackBuffer(w, &pixels, &width, &height, &rowBytes);
	fill(pixels, rowBytes, (SLSIntRect){ 0, 0, (int32_t)width, (int32_t)height }, color);
	SLSWindowCommit(w, (SLSIntRect[]){ { 0, 0, (int32_t)width, (int32_t)height } }, 1);
	SLSOrderWindow(w, kSLSOrderAbove, 0);
	return w;
}

static int
drawMain(void)
{
	SLSWindowID red = solidWindow((SLSRect){ 100, 100, 300, 200 }, 0xffff0000);
	SLSWindowID blue = solidWindow((SLSRect){ 250, 200, 300, 200 }, 0xff0000ff);
	SLSWindowID green = solidWindow((SLSRect){ 600, 100, 100, 100 }, 0xff00ff00);
	const char *path = "/tmp/ws-draw.cmd";

	SLSSetWindowAlpha(green, 0.5);
	printf("drawn %u %u %u\n", red, blue, green);
	fflush(stdout);
	for (;;) {
		char line[64] = "";
		double x, y;
		FILE *f = fopen(path, "r");

		if (f == NULL) {
			usleep(20000);
			continue;
		}
		if (fgets(line, sizeof(line), f) == NULL)
			line[0] = '\0';
		fclose(f);
		if (strncmp(line, "quit", 4) == 0) {
			unlink(path);
			return 0;
		}
		if (strncmp(line, "raise-red", 9) == 0)
			SLSOrderWindow(red, kSLSOrderAbove, 0);
		else if (sscanf(line, "move-blue %lf %lf", &x, &y) == 2)
			SLSSetWindowFrame(blue, (SLSRect){ x, y, 300, 200 });
		else if (sscanf(line, "cursor %lf %lf", &x, &y) == 2)
			SLSWarpCursorPosition((SLSPoint){ x, y });
		unlink(path);
	}
}

int
main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "child") == 0)
		return childMain();
	if (argc > 1 && strcmp(argv[1], "draw") == 0)
		return drawMain();
	if (argc > 1 && strcmp(argv[1], "hotkeys") == 0)
		return hotkeysMain();
	if (argc > 1 && strcmp(argv[1], "events") == 0)
		return eventsMain(argc > 2 && strcmp(argv[2], "grab") == 0);
	setvbuf(stdout, NULL, _IOLBF, 0);
	testConnection();
	testDisplays();
	testModes();
	testGamma();
	testMirrorAndFade();
	testCapture();
	testCursor();
	testWindows();
	testStatusArea();
	testSurfaces();
	testEvents();
	testClientExit(argv[0]);
	printf("skylight-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
