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
 * cstest: what the code integrity policy made of this process
 * (Docs/ADR/0001-Kernel-Entitlements.md).
 *
 *   cstest status         code signing flags: valid, signed, platform
 *   cstest entitlements   the entitlements the kernel holds for it
 *   cstest hid            whether IOHIDSystem accepts it as its server
 *                         (a restricted entitlement), run as nobody
 *
 * The build installs one copy in the trust cache (/usr/local/bin/cstest)
 * and four outside it, in /usr/local/tests/codesign/outside: re-signed
 * with the same entitlements (its exec is refused), ad hoc with an
 * unrestricted entitlement only, ad hoc with a third-party app's identity
 * and profile entitlements (they run, with no privilege), and unsigned.
 */

#include <IOKit/IOKitLib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

/* <sys/codesign.h> and <kern/cs_blobs.h> are not in the SDK. */
int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);
#define CS_OPS_STATUS		0
#define CS_OPS_ENTITLEMENTS_BLOB 7
#define CS_VALID		0x00000001
#define CS_ADHOC		0x00000002
#define CS_PLATFORM_BINARY	0x04000000
#define CS_SIGNED		0x20000000

/* IOHIDShared.h */
#define kIOHIDServerConnectType	0

static int
status(void)
{
	uint32_t flags = 0;

	if (csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags)) != 0) {
		perror("csops");
		return 1;
	}
	printf("flags=0x%08x valid=%d adhoc=%d signed=%d platform=%d\n", flags,
	    !!(flags & CS_VALID), !!(flags & CS_ADHOC), !!(flags & CS_SIGNED),
	    !!(flags & CS_PLATFORM_BINARY));
	return 0;
}

static int
entitlements(void)
{
	static char buf[65536];
	uint32_t length;

	memset(buf, 0, sizeof(buf));
	if (csops(getpid(), CS_OPS_ENTITLEMENTS_BLOB, buf, sizeof(buf) - 1) != 0) {
		/* The kernel answers EINVAL for a process that is not signed. */
		printf("entitlements: none (%s)\n", strerror(errno));
		return 0;
	}
	memcpy(&length, buf + 4, sizeof(length));
	length = ntohl(length);
	if (length <= 8) {
		printf("entitlements: none\n");
		return 0;
	}
	if (length > sizeof(buf) - 1) {
		length = sizeof(buf) - 1;
	}
	fwrite(buf + 8, 1, length - 8, stdout);
	printf("\n");
	return 0;
}

static int
hid(void)
{
	io_service_t service;
	io_connect_t connect = IO_OBJECT_NULL;
	kern_return_t kr;

	/* As nobody, so root's privilege does not open the connection. */
	if (getuid() == 0 && (setgid(4294967294U) != 0 || setuid(4294967294U) != 0)) {
		perror("setuid");
		return 1;
	}
	service = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceMatching("IOHIDSystem"));
	if (service == IO_OBJECT_NULL) {
		printf("hid: no IOHIDSystem\n");
		return 1;
	}
	kr = IOServiceOpen(service, mach_task_self(), kIOHIDServerConnectType, &connect);
	IOObjectRelease(service);
	if (connect != IO_OBJECT_NULL) {
		IOServiceClose(connect);
	}
	/* Busy: entitled, but the window server holds the connection. */
	printf("hid: 0x%x %s\n", kr,
	    (kr == KERN_SUCCESS || kr == kIOReturnBusy) ? "accepted" : "refused");
	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && strcmp(argv[1], "status") == 0) {
		return status();
	}
	if (argc == 2 && strcmp(argv[1], "entitlements") == 0) {
		return entitlements();
	}
	if (argc == 2 && strcmp(argv[1], "hid") == 0) {
		return hid();
	}
	fprintf(stderr, "usage: cstest status | entitlements | hid\n");
	return 2;
}
