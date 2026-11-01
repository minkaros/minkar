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
#include <stdio.h>
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

int
main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	testConnection();
	testDisplays();
	testModes();
	testGamma();
	printf("skylight-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
