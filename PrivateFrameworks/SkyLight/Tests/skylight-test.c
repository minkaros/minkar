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

#include <stdio.h>
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

int
main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	testConnection();
	testDisplays();
	printf("skylight-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
