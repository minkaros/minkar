/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * iosurface-test: IOSurface.framework end to end (roadmap step 4): a
 * surface made and described, found again by its ID and through a Mach
 * port, its pixels shared with another process, its values, seed and use
 * count across processes, planes, and its life: a surface stays while any
 * process holds it, and goes with the last. One line per check, "ok" or
 * "FAIL", then "iosurface-test: N passed, M failed".
 *
 *	iosurface-test			all of it
 *	iosurface-test child ID		(the other process: checks and marks)
 *	iosurface-test hold ID SECONDS	(holds the surface a while)
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static int passed, failed;

static void check(int good, const char *what) {
	printf("%s %s\n", good ? "ok" : "FAIL", what);
	if (good)
		passed++;
	else
		failed++;
}

static void setNumber(CFMutableDictionaryRef d, CFStringRef key, int value) {
	CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &value);
	CFDictionarySetValue(d, key, n);
	CFRelease(n);
}

static CFMutableDictionaryRef properties(int width, int height, int bpe) {
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	setNumber(d, kIOSurfaceWidth, width);
	setNumber(d, kIOSurfaceHeight, height);
	setNumber(d, kIOSurfaceBytesPerElement, bpe);
	setNumber(d, kIOSurfacePixelFormat, 'BGRA');
	return d;
}

/* The other process: the pixels are the parent's; it marks its own row,
 * sets a value and takes a use. */
static int child(IOSurfaceID sid) {
	IOSurfaceRef s = IOSurfaceLookup(sid);
	if (s == NULL)
		return 2;
	uint32_t *px = IOSurfaceGetBaseAddress(s);
	if (IOSurfaceGetWidth(s) != 64 || px[0] != 0xff112233 || px[64 * 31 + 63] != 0xff445566)
		return 3;
	px[IOSurfaceGetBytesPerRow(s) / 4 * 5] = 0xffabcdef;
	IOSurfaceSetValue(s, CFSTR("child"), CFSTR("yes"));
	IOSurfaceIncrementUseCount(s);
	IOSurfaceLock(s, 0, NULL);
	IOSurfaceUnlock(s, 0, NULL);
	CFRelease(s);
	return 0;
}

static int hold(IOSurfaceID sid, int seconds) {
	IOSurfaceRef s = IOSurfaceLookup(sid);
	if (s == NULL)
		return 2;
	printf("held\n");
	fflush(stdout);
	sleep(seconds);
	CFRelease(s);
	return 0;
}

/* A process with no privileges (nobody): 0 if it finds the surface by its
 * ID, 2 if not. */
static int stranger(IOSurfaceID sid) {
	if (setgid((gid_t)-2) != 0 || setuid((uid_t)-2) != 0)
		return 4;
	IOSurfaceRef s = IOSurfaceLookup(sid);
	if (s == NULL)
		return 2;
	CFRelease(s);
	return 0;
}

static pid_t spawn(const char *self, char *const argv[], int *out) {
	posix_spawn_file_actions_t actions;
	int pipes[2];
	pid_t pid;

	pipe(pipes);
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, pipes[1], 1);
	posix_spawn_file_actions_addclose(&actions, pipes[0]);
	if (posix_spawn(&pid, self, &actions, NULL, argv, environ) != 0)
		pid = -1;
	posix_spawn_file_actions_destroy(&actions);
	close(pipes[1]);
	*out = pipes[0];
	return pid;
}

int main(int argc, char *argv[]) {
	if (argc == 3 && strcmp(argv[1], "child") == 0)
		return child((IOSurfaceID)strtoul(argv[2], NULL, 10));
	if (argc == 4 && strcmp(argv[1], "hold") == 0)
		return hold((IOSurfaceID)strtoul(argv[2], NULL, 10), atoi(argv[3]));
	if (argc == 3 && strcmp(argv[1], "stranger") == 0)
		return stranger((IOSurfaceID)strtoul(argv[2], NULL, 10));

	CFMutableDictionaryRef p = properties(64, 32, 4);
	IOSurfaceRef s = IOSurfaceCreate(p);
	CFRelease(p);
	check(s != NULL && CFGetTypeID(s) == IOSurfaceGetTypeID(), "a surface is made");
	if (s == NULL)
		goto done;
	check(IOSurfaceGetID(s) != 0, "it has a global ID (the registry answers)");
	check(IOSurfaceGetWidth(s) == 64 && IOSurfaceGetHeight(s) == 32 &&
	    IOSurfaceGetBytesPerElement(s) == 4 && IOSurfaceGetPixelFormat(s) == 'BGRA',
	    "width, height, bytes per element and pixel format");
	check(IOSurfaceGetBytesPerRow(s) >= 256 && IOSurfaceGetBytesPerRow(s) % 16 == 0 &&
	    IOSurfaceGetAllocSize(s) >= IOSurfaceGetBytesPerRow(s) * 32,
	    "rows aligned to 16 bytes, enough memory");
	check(IOSurfaceGetPlaneCount(s) == 0 && IOSurfaceGetWidthOfPlane(s, 0) == 64,
	    "no planes; plane 0 is the surface itself");

	uint32_t *px = IOSurfaceGetBaseAddress(s);
	size_t stride = IOSurfaceGetBytesPerRow(s) / 4;
	px[0] = 0xff112233;
	px[64 * 31 + 63] = 0xff445566;	/* inside, whatever the stride */

	IOSurfaceRef again = IOSurfaceLookup(IOSurfaceGetID(s));
	check(again != NULL && again != s && IOSurfaceGetBaseAddress(again) != (void *)px &&
	    ((uint32_t *)IOSurfaceGetBaseAddress(again))[0] == 0xff112233,
	    "found by its ID: another mapping of the same pixels");
	if (again)
		CFRelease(again);

	mach_port_t port = IOSurfaceCreateMachPort(s);
	IOSurfaceRef viaPort = IOSurfaceLookupFromMachPort(port);
	check(port != MACH_PORT_NULL && viaPort != NULL &&
	    IOSurfaceGetID(viaPort) == IOSurfaceGetID(s), "and through its Mach port");
	if (viaPort)
		CFRelease(viaPort);
	mach_port_deallocate(mach_task_self(), port);

	/* Seed: a writing lock changes it, a read-only one does not. */
	uint32_t seed0 = IOSurfaceGetSeed(s), seed;
	IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
	IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, &seed);
	check(seed == seed0, "a read-only lock keeps the seed");
	IOSurfaceLock(s, 0, NULL);
	IOSurfaceUnlock(s, 0, &seed);
	check(seed == seed0 + 1 && IOSurfaceGetSeed(s) == seed0 + 1, "a writing lock changes it");
	check(IOSurfaceUnlock(s, 0, NULL) != KERN_SUCCESS, "unlocking an unlocked surface fails");

	/* Values. */
	IOSurfaceSetValue(s, CFSTR("name"), CFSTR("test"));
	CFTypeRef v = IOSurfaceCopyValue(s, CFSTR("name"));
	check(v != NULL && CFEqual(v, CFSTR("test")), "a value set and read back");
	if (v)
		CFRelease(v);

	/* Another process: same pixels, its mark, its value, its use. */
	char sidText[16];
	snprintf(sidText, sizeof(sidText), "%u", IOSurfaceGetID(s));
	int out;
	char *childArgs[] = { argv[0], "child", sidText, NULL };
	pid_t pid = spawn(argv[0], childArgs, &out);
	int status = -1;
	waitpid(pid, &status, 0);
	close(out);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "another process finds it by its ID, with this one's pixels");
	check(px[stride * 5] == 0xffabcdef, "and what it writes shows here");
	v = IOSurfaceCopyValue(s, CFSTR("child"));
	check(v != NULL && CFEqual(v, CFSTR("yes")), "its values are shared");
	if (v)
		CFRelease(v);
	CFDictionaryRef all = IOSurfaceCopyAllValues(s);
	check(all != NULL && CFDictionaryGetCount(all) == 2, "all the values, from both");
	if (all)
		CFRelease(all);
	IOSurfaceRemoveValue(s, CFSTR("name"));
	v = IOSurfaceCopyValue(s, CFSTR("name"));
	check(v == NULL, "a value removed");
	check(IOSurfaceGetUseCount(s) == 1 && IOSurfaceIsInUse(s), "the use count, across processes");
	check(IOSurfaceGetSeed(s) == seed0 + 2, "and the seed");
	IOSurfaceDecrementUseCount(s);

	/* Life: held by another process, it stays after this one lets go; it
	 * goes with the last holder. */
	IOSurfaceID sid = IOSurfaceGetID(s);
	char *holdArgs[] = { argv[0], "hold", sidText, "3", NULL };
	pid = spawn(argv[0], holdArgs, &out);
	char line[16] = "";
	read(out, line, sizeof(line) - 1);
	CFRelease(s);
	IOSurfaceRef stays = IOSurfaceLookup(sid);
	check(stays != NULL, "it stays while another process holds it");
	if (stays)
		CFRelease(stays);
	waitpid(pid, &status, 0);
	close(out);
	usleep(500000);
	IOSurfaceRef gone = IOSurfaceLookup(sid);
	check(gone == NULL, "and goes with the last holder");
	if (gone)
		CFRelease(gone);

	/* An ID is no secret: a process without privileges does not find
	 * another's surface by it, unless the surface is global. */
	for (int global = 0; global < 2; global++) {
		CFMutableDictionaryRef gp = properties(16, 16, 4);
		if (global)
			CFDictionarySetValue(gp, kIOSurfaceIsGlobal, kCFBooleanTrue);
		IOSurfaceRef g = IOSurfaceCreate(gp);
		CFRelease(gp);
		char gid[16];
		snprintf(gid, sizeof(gid), "%u", g ? IOSurfaceGetID(g) : 0);
		char *strangerArgs[] = { argv[0], "stranger", gid, NULL };
		pid = spawn(argv[0], strangerArgs, &out);
		status = 0;
		waitpid(pid, &status, 0);
		close(out);
		if (global)
			check(g != NULL && WIFEXITED(status) && WEXITSTATUS(status) == 0,
			    "anyone finds a global surface by its ID");
		else
			check(g != NULL && WIFEXITED(status) && WEXITSTATUS(status) == 2,
			    "a process without privileges does not find another's surface by its ID");
		if (g)
			CFRelease(g);
	}

	/* Planes: a 4:2:0 layout, luma then chroma. */
	CFMutableDictionaryRef pp = properties(64, 32, 1);
	CFMutableDictionaryRef luma = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFMutableDictionaryRef chroma = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	setNumber(luma, kIOSurfacePlaneWidth, 64);
	setNumber(luma, kIOSurfacePlaneHeight, 32);
	setNumber(luma, kIOSurfacePlaneBytesPerElement, 1);
	setNumber(chroma, kIOSurfacePlaneWidth, 32);
	setNumber(chroma, kIOSurfacePlaneHeight, 16);
	setNumber(chroma, kIOSurfacePlaneBytesPerElement, 2);
	const void *planes[] = { luma, chroma };
	CFArrayRef info = CFArrayCreate(NULL, planes, 2, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(pp, kIOSurfacePlaneInfo, info);
	IOSurfaceRef yuv = IOSurfaceCreate(pp);
	check(yuv != NULL && IOSurfaceGetPlaneCount(yuv) == 2 &&
	    IOSurfaceGetWidthOfPlane(yuv, 1) == 32 && IOSurfaceGetBytesPerElementOfPlane(yuv, 1) == 2 &&
	    (char *)IOSurfaceGetBaseAddressOfPlane(yuv, 1) >=
	    (char *)IOSurfaceGetBaseAddressOfPlane(yuv, 0) + 64 * 32 &&
	    IOSurfaceGetSubsampling(yuv) == kIOSurfaceSubsampling420,
	    "planes: their sizes, places and 4:2:0 subsampling");
	if (yuv)
		CFRelease(yuv);
	CFRelease(info);
	CFRelease(luma);
	CFRelease(chroma);
	CFRelease(pp);

	check(IOSurfaceCreate(NULL) == NULL, "no properties, no surface");
	check(IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, 257) == 272 &&
	    IOSurfaceGetPropertyMaximum(kIOSurfaceWidth) >= 8192, "alignment and limits");
done:
	printf("iosurface-test: %d passed, %d failed\n", passed, failed);
	return failed;
}
