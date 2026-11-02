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
 * wsctl: what the new window server is doing (WindowServer plan, section
 * 4, Debug/, and section 6). Through SkyLight, as any client.
 *
 *   wsctl stats      frames, idle display link ticks, commits, input
 *                    events, damaged area and frame times
 *   wsctl windows    the window list, front to back
 *   wsctl displays   each display's bounds, mode and state
 *   wsctl modes      the main display's modes, the current one marked *
 *   wsctl mode ID [permanent]
 *                    sets the main display's mode for the session, or
 *                    saves it for the next ones too
 *   wsctl restore    every display as saved permanently
 *   wsctl front      the front app's process and the key window
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SkyLight/SkyLight.h>

static int
stats(void)
{
	SLSServerStats st;

	if (SLSGetServerStats(&st) != kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	printf("frames %llu\n", (unsigned long long)st.frames);
	printf("idle_ticks %llu\n", (unsigned long long)st.idleTicks);
	printf("commits %llu\n", (unsigned long long)st.commits);
	printf("events %llu\n", (unsigned long long)st.events);
	printf("damaged_pixels %llu\n", (unsigned long long)st.damagedPixels);
	printf("last_damaged_pixels %llu\n", (unsigned long long)st.lastDamagedPixels);
	printf("last_frame_us %llu\n", (unsigned long long)st.lastFrameMicros);
	printf("mean_frame_us %llu\n",
	    (unsigned long long)(st.frames ? st.totalFrameMicros / st.frames : 0));
	return 0;
}

static int
windows(void)
{
	static SLSWindowID list[kSLSMaxWindowList];
	uint32_t n = 0;

	if (SLSWindowListCopy(kSLSWindowListAll, 0, kSLSMaxWindowList, list, &n) !=
	    kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	printf("%-6s %-6s %-11s %-6s %-22s %-5s %s\n", "ID", "PID", "LEVEL", "SHOWN",
	    "FRAME", "ALPHA", "OWNER");
	for (uint32_t i = 0; i < n; i++) {
		SLSWindowInfo w;
		char frame[32];

		if (SLSGetWindowInfo(list[i], &w) != kSLSErrorSuccess)
			continue;
		snprintf(frame, sizeof(frame), "%.0f,%.0f %.0fx%.0f", w.x, w.y, w.width, w.height);
		printf("%-6u %-6d %-11d %-6s %-22s %-5.2f %s\n", w.window, w.ownerPID, w.level,
		    (w.flags & kSLSWindowOnScreen) ? "yes" : "no", frame, w.alpha, w.ownerName);
	}
	return 0;
}

static int
displays(void)
{
	SLSDisplayID list[32];
	uint32_t n = 0;

	if (SLSGetOnlineDisplayList(32, list, &n) != kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	for (uint32_t i = 0; i < n; i++) {
		SLSDisplayInfo d;
		SLSDisplayMode m;

		if (SLSGetDisplayInfo(list[i], &d) != kSLSErrorSuccess ||
		    SLSGetCurrentDisplayMode(list[i], &m) != kSLSErrorSuccess)
			continue;
		printf("0x%x %.0f,%.0f %.0fx%.0f mode %u %u bpp %.0f Hz flags 0x%x\n", d.display,
		    d.x, d.y, d.width, d.height, m.modeID, m.depth, m.refreshRate, d.flags);
	}
	return 0;
}

static int
front(void)
{
	pid_t pid = 0;

	if (SLSGetFrontProcess(&pid) != kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	printf("front pid=%d key window=%u\n", (int)pid, SLSGetKeyWindow());
	return 0;
}

/* The main display's modes, the current one marked. */
static int
modes(void)
{
	static SLSDisplayMode list[kSLSMaxDisplayModes];
	SLSDisplayMode current;
	SLSDisplayID display;
	uint32_t n = 0, count = 0;

	if (SLSGetOnlineDisplayList(1, &display, &n) != kSLSErrorSuccess || n == 0 ||
	    SLSGetCurrentDisplayMode(display, &current) != kSLSErrorSuccess ||
	    SLSCopyDisplayModes(display, kSLSMaxDisplayModes, list, &count) !=
	    kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	for (uint32_t i = 0; i < count; i++)
		printf("%c %u %ux%u\n", list[i].modeID == current.modeID ? '*' : ' ',
		    list[i].modeID, list[i].pixelWidth, list[i].pixelHeight);
	return 0;
}

/* Sets the main display's mode for the session, or permanently. */
static int
setMode(const char *arg, uint32_t option)
{
	SLSDisplayConfigRef config;
	SLSDisplayID display;
	uint32_t n = 0;
	int err;

	if (SLSGetOnlineDisplayList(1, &display, &n) != kSLSErrorSuccess || n == 0 ||
	    SLSBeginDisplayConfiguration(&config) != kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	if ((err = SLSConfigureDisplayWithDisplayMode(config, display,
	    (uint32_t)strtoul(arg, NULL, 0))) != kSLSErrorSuccess) {
		SLSCancelDisplayConfiguration(config);
		fprintf(stderr, "wsctl: no such mode: %s (%d)\n", arg, err);
		return 1;
	}
	if ((err = SLSCompleteDisplayConfiguration(config, option)) !=
	    kSLSErrorSuccess) {
		fprintf(stderr, "wsctl: the mode was not set (%d)\n", err);
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && strcmp(argv[1], "stats") == 0)
		return stats();
	if (argc == 2 && strcmp(argv[1], "windows") == 0)
		return windows();
	if (argc == 2 && strcmp(argv[1], "displays") == 0)
		return displays();
	if (argc == 2 && strcmp(argv[1], "modes") == 0)
		return modes();
	if (argc == 3 && strcmp(argv[1], "mode") == 0)
		return setMode(argv[2], kSLSConfigureForSession);
	if (argc == 4 && strcmp(argv[1], "mode") == 0 && strcmp(argv[3], "permanent") == 0)
		return setMode(argv[2], kSLSConfigurePermanently);
	if (argc == 2 && strcmp(argv[1], "restore") == 0) {
		if (SLSRestorePermanentDisplayConfiguration() == kSLSErrorSuccess)
			return 0;
		fprintf(stderr, "wsctl: no window server\n");
		return 1;
	}
	if (argc == 2 && strcmp(argv[1], "front") == 0)
		return front();
	fprintf(stderr, "usage: wsctl stats | windows | displays | modes | mode ID [permanent] | restore | front\n");
	return 2;
}
