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
 * eventtap-test: CoreGraphics' events, taps and CGEventPost through the
 * new window server (WindowServer plan, WS3). One line per check, "ok" or
 * "FAIL", then "eventtap-test: N passed, M failed"; the exit status is the
 * number of failures. Needs the new server up and root (posting and
 * filtering taps). "eventtap-test listen" prints what a listen-only tap
 * sees until it is killed, for the local input test.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGEvent.h>
#include <SkyLight/SkyLight.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

/* ---- The taps' thread: its own run loop, so this process can post while
 * its filtering tap is asked. ---- */

static CFRunLoopRef tapLoop;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

static void *
tapThread(void *arg)
{
	pthread_mutex_lock(&lock);
	tapLoop = CFRunLoopGetCurrent();
	CFRetain(tapLoop);
	pthread_cond_signal(&ready);
	pthread_mutex_unlock(&lock);
	/* A source keeps the loop running until there are taps. */
	for (;;)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
	return NULL;
}

static void
startTapThread(void)
{
	pthread_t t;

	pthread_create(&t, NULL, tapThread, NULL);
	pthread_mutex_lock(&lock);
	while (tapLoop == NULL)
		pthread_cond_wait(&ready, &lock);
	pthread_mutex_unlock(&lock);
}

static void
addTap(CFMachPortRef tap)
{
	CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL, tap, 0);
	CFRunLoopAddSource(tapLoop, source, kCFRunLoopDefaultMode);
	CFRelease(source);
	CFRunLoopWakeUp(tapLoop);
}

static volatile int listened, listenedKey = -1;
static volatile int disabledSeen;

static CGEventRef
listenCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *info)
{
	if (type == kCGEventKeyDown) {
		listenedKey = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
		listened++;
	}
	return event;
}

/* Drops 'b' (key code 11), lets everything else through. */
static CGEventRef
filterCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *info)
{
	if ((type == kCGEventKeyDown || type == kCGEventKeyUp) &&
	    CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode) == 11)
		return NULL;
	return event;
}

/* Takes too long to answer. */
static CGEventRef
slowCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *info)
{
	if (type == (CGEventType)kCGEventTapDisabledByTimeout) {
		disabledSeen = 1;
		return event;
	}
	sleep(2);
	return event;
}

/* ---- This process's key window, to see where posted events go ---- */

static SLSWindowID window;

static void
makeKeyWindow(void)
{
	SLSNewWindow(0, (SLSRect){ 700, 500, 100, 100 }, &window);
	SLSOrderWindow(window, kSLSOrderAbove, 0);
	SLSSetKeyWindow(window);
}

/* The key-down events that reached the window within ms milliseconds, as
 * key codes in buf. */
static int
keysReceived(int ms, char *buf, size_t size)
{
	SLSEvent events[kSLSMaxEvents];
	int n = 0;

	buf[0] = '\0';
	for (int t = 0; t < ms; t += 50) {
		uint32_t count = 0;
		SLSReceiveEvents(events, kSLSMaxEvents, &count, 0);
		for (uint32_t i = 0; i < count; i++)
			if (events[i].type == kSLSEventKeyDown && events[i].window == window) {
				size_t len = strlen(buf);
				snprintf(buf + len, size - len, "%s%s", len ? "," : "",
				    events[i].characters);
				n++;
			}
		usleep(50000);
	}
	return n;
}

static void
post(CGKeyCode key)
{
	CGEventRef down = CGEventCreateKeyboardEvent(NULL, key, true);
	CGEventRef up = CGEventCreateKeyboardEvent(NULL, key, false);

	CGEventPost(kCGSessionEventTap, down);
	CGEventPost(kCGSessionEventTap, up);
	CFRelease(down);
	CFRelease(up);
}

static void
testEvents(void)
{
	char detail[96];
	CGEventRef e = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, CGPointMake(12, 34),
	    kCGMouseButtonLeft);
	CGPoint p = CGEventGetLocation(e);

	check(e != NULL && CFGetTypeID(e) == CGEventGetTypeID() &&
	    CGEventGetType(e) == kCGEventLeftMouseDown && p.x == 12 && p.y == 34 &&
	    CGEventGetIntegerValueField(e, kCGMouseEventClickState) == 1,
	    "a mouse event is made", NULL);
	CGEventSetIntegerValueField(e, kCGEventSourceUserData, 42);
	CFDataRef data = CGEventCreateData(NULL, e);
	CGEventRef back = CGEventCreateFromData(NULL, data);
	check(back != NULL && CGEventGetType(back) == kCGEventLeftMouseDown &&
	    CGEventGetIntegerValueField(back, kCGEventSourceUserData) == 42,
	    "it goes through data and back", NULL);
	CGEventRef copy = CGEventCreateCopy(e);
	CGEventSetLocation(copy, CGPointMake(5, 6));
	check(CGEventGetLocation(e).x == 12 && CGEventGetLocation(copy).x == 5,
	    "a copy is its own", NULL);
	CFRelease(e);
	CFRelease(data);
	CFRelease(back);
	CFRelease(copy);

	CGEventRef k = CGEventCreateKeyboardEvent(NULL, 0, true);
	UniChar u[2] = { 'x', 0 };
	UniCharCount n = 0;
	CGEventKeyboardSetUnicodeString(k, 1, u);
	u[0] = 0;
	CGEventKeyboardGetUnicodeString(k, 2, &n, u);
	snprintf(detail, sizeof(detail), "%lu character(s)", (unsigned long)n);
	check(CGEventGetType(k) == kCGEventKeyDown &&
	    CGEventGetIntegerValueField(k, kCGKeyboardEventKeycode) == 0 && n == 1 && u[0] == 'x',
	    "a key event carries its key code and characters", detail);
	CFRelease(k);

	CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	CGEventSourceSetUserData(source, 7);
	CGEventRef s = CGEventCreate(source);
	check(CGEventSourceGetSourceStateID(source) == kCGEventSourceStateHIDSystemState &&
	    CGEventGetIntegerValueField(s, kCGEventSourceUserData) == 7,
	    "an event takes its source's user data", NULL);
	CFRelease(s);
	CFRelease(source);
}

static void
testPostAndTaps(void)
{
	char keys[64], detail[96];
	CGEventTapInformation list[16];
	uint32_t count = 0;
	int n, found = 0;

	makeKeyWindow();
	keysReceived(200, keys, sizeof(keys));	/* nothing left over */
	post(8);	/* c */
	n = keysReceived(500, keys, sizeof(keys));
	check(n == 1 && strcmp(keys, "c") == 0, "a posted key reaches the key window, "
	    "with the keymap's characters", keys);

	CFMachPortRef listen = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
	    kCGEventTapOptionListenOnly, CGEventMaskBit(kCGEventKeyDown), listenCallback, NULL);
	check(listen != NULL, "a listen-only tap is made", NULL);
	addTap(listen);
	CGGetEventTapList(16, list, &count);
	for (uint32_t i = 0; i < count; i++)
		if (list[i].tappingProcess == getpid() && list[i].options == kCGEventTapOptionListenOnly &&
		    list[i].eventsOfInterest == CGEventMaskBit(kCGEventKeyDown) && list[i].enabled)
			found = 1;
	snprintf(detail, sizeof(detail), "%u tap(s)", count);
	check(found, "it is in the tap list", detail);
	post(9);	/* v */
	n = keysReceived(500, keys, sizeof(keys));
	snprintf(detail, sizeof(detail), "%d seen, key code %d", listened, listenedKey);
	check(listened == 1 && listenedKey == 9, "it sees a posted key", detail);
	check(n == 1 && strcmp(keys, "v") == 0, "which still reaches the window", keys);
	CGEventTapEnable(listen, false);
	post(9);
	keysReceived(300, keys, sizeof(keys));
	check(!CGEventTapIsEnabled(listen) && listened == 1, "a disabled tap sees nothing", NULL);

	/* At the session location: posted events enter there, past HID taps. */
	CFMachPortRef filter = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
	    kCGEventTapOptionDefault, CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp),
	    filterCallback, NULL);
	check(filter != NULL, "a filtering tap is made", NULL);
	addTap(filter);
	post(11);	/* b, dropped */
	post(8);	/* c */
	n = keysReceived(500, keys, sizeof(keys));
	check(n == 1 && strcmp(keys, "c") == 0, "a filtering tap drops one key and passes another",
	    keys);
	CGEventTapEnable(filter, false);

	CFMachPortRef slow = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
	    kCGEventTapOptionDefault, CGEventMaskBit(kCGEventKeyDown), slowCallback, NULL);
	addTap(slow);
	post(8);
	n = keysReceived(3000, keys, sizeof(keys));
	check(disabledSeen && !CGEventTapIsEnabled(slow), "a tap that does not answer is disabled",
	    NULL);
	check(n == 1, "and the event goes on", keys);
	SLSReleaseWindow(window);
}

static CGEventRef
printCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *info)
{
	CGPoint p = CGEventGetLocation(event);

	printf("tap type=%u key=%lld x=%.0f y=%.0f\n", type,
	    CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode), p.x, p.y);
	return event;
}

static int
listenMain(void)
{
	CFMachPortRef tap = CGEventTapCreate(kCGHIDEventTap, kCGTailAppendEventTap,
	    kCGEventTapOptionListenOnly, CGEventMaskBit(kCGEventKeyDown) |
	    CGEventMaskBit(kCGEventLeftMouseDown), printCallback, NULL);

	if (tap == NULL)
		return 1;
	addTap(tap);
	return 0;
}

/* The input state (WS4a): a posted key is down in the combined session
 * state and not in the hardware's; the counters and times follow. */
static void
testSourceState(void)
{
	const CGKeyCode key = 37;	/* l */
	CGEventRef down = CGEventCreateKeyboardEvent(NULL, key, true);
	CGEventRef up = CGEventCreateKeyboardEvent(NULL, key, false);
	uint32_t before = CGEventSourceCounterForEventType(kCGEventSourceStateCombinedSessionState,
	    kCGEventKeyDown);

	CGEventPost(kCGSessionEventTap, down);
	usleep(100000);
	check(CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, key),
	    "a posted key is down in the combined session state", NULL);
	check(!CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, key),
	    "and not in the hardware's", NULL);
	check(CGEventSourceCounterForEventType(kCGEventSourceStateCombinedSessionState,
	    kCGEventKeyDown) == before + 1, "the key down is counted", NULL);
	check(CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateCombinedSessionState,
	    kCGEventKeyDown) < 2, "and recent", NULL);
	CGEventPost(kCGSessionEventTap, up);
	usleep(100000);
	check(!CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, key),
	    "and up again", NULL);
	check(!CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft),
	    "no button is down", NULL);
	CFRelease(down);
	CFRelease(up);
}

int
main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	startTapThread();
	if (argc > 1 && strcmp(argv[1], "listen") == 0) {
		if (listenMain())
			return 1;
		printf("listening\n");
		for (;;)
			pause();
	}
	testEvents();
	testPostAndTaps();
	testSourceState();
	printf("eventtap-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
