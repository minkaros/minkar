/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * hidevents SECONDS: opens IOHIDSystem as the event server does
 * (kIOHIDServerConnectType), creates and maps the shared memory, registers
 * a notification port and prints every event of the low-level queue for
 * SECONDS seconds, one line each:
 *
 *   event type=T x=X y=Y key=K button=B
 *
 * It starts with "open=0x..." (the IOServiceOpen result) and, once the
 * queue is ready, "ready"; it ends with "kicks=N events=M".
 */
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>

static void
print_event(volatile NXEvent *e)
{
	int key = -1, button = -1;

	switch (e->type) {
	case NX_KEYDOWN:
	case NX_KEYUP:
		key = e->data.key.keyCode;
		break;
	case NX_LMOUSEDOWN: case NX_LMOUSEUP:
	case NX_RMOUSEDOWN: case NX_RMOUSEUP:
	case NX_OMOUSEDOWN: case NX_OMOUSEUP:
		button = e->data.mouse.buttonNumber;
		break;
	}
	printf("event type=%d x=%d y=%d key=%d button=%d\n", (int)e->type,
	    (int)e->location.x, (int)e->location.y, key, button);
}

int
main(int argc, char **argv)
{
	int seconds = argc > 1 ? atoi(argv[1]) : 10;
	io_service_t service;
	io_connect_t conn = IO_OBJECT_NULL;
	kern_return_t kr;

	setvbuf(stdout, NULL, _IOLBF, 0);
	service = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceMatching(kIOHIDSystemClass));
	if (!service) {
		printf("no IOHIDSystem\n");
		return 2;
	}
	kr = IOServiceOpen(service, mach_task_self(), kIOHIDServerConnectType, &conn);
	IOObjectRelease(service);
	printf("open=0x%x\n", kr);
	if (kr != KERN_SUCCESS)
		return 3;

	/* Method 0: create the shared memory (its version). */
	uint64_t version = kIOHIDCurrentShmemVersion;
	kr = IOConnectCallScalarMethod(conn, 0, &version, 1, NULL, NULL);
	if (kr != KERN_SUCCESS) {
		printf("createShmem=0x%x\n", kr);
		return 4;
	}
	mach_vm_address_t addr = 0;
	mach_vm_size_t size = 0;
	kr = IOConnectMapMemory64(conn, kIOHIDGlobalMemory, mach_task_self(),
	    &addr, &size, kIOMapAnywhere);
	if (kr != KERN_SUCCESS) {
		printf("map=0x%x\n", kr);
		return 5;
	}
	volatile EvOffsets *eop = (volatile EvOffsets *)(uintptr_t)addr;
	volatile EvGlobals *evg = (volatile EvGlobals *)(uintptr_t)(addr + eop->evGlobalsOffset);

	mach_port_t port = MACH_PORT_NULL;
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	kr = IOConnectSetNotificationPort(conn, kIOHIDEventNotification, port, 0);
	if (kr != KERN_SUCCESS) {
		printf("port=0x%x\n", kr);
		return 6;
	}
	printf("ready size=%llu\n", (unsigned long long)size);

	mach_timebase_info_data_t tb;
	mach_timebase_info(&tb);
	uint64_t end = mach_absolute_time() +
	    (uint64_t)seconds * 1000000000ULL * tb.denom / tb.numer;
	int kicks = 0, events = 0;

	for (;;) {
		uint64_t now = mach_absolute_time();
		if (now >= end)
			break;
		uint64_t left_ms = (end - now) * tb.numer / tb.denom / 1000000ULL;
		struct {
			mach_msg_header_t h;
			mach_msg_trailer_t t;
		} msg;
		kr = mach_msg(&msg.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
		    port, (mach_msg_timeout_t)(left_ms ? left_ms : 1), MACH_PORT_NULL);
		if (kr == MACH_RCV_TIMED_OUT)
			break;
		if (kr != KERN_SUCCESS) {
			printf("receive=0x%x\n", kr);
			break;
		}
		kicks++;
		/* Read from LLEHead up to LLETail, as the event server does. */
		while (evg->LLEHead != evg->LLETail) {
			volatile NXEQElement *el = &evg->lleq[evg->LLEHead];
			print_event(&el->event);
			events++;
			evg->LLEHead = el->next;
		}
	}
	printf("kicks=%d events=%d\n", kicks, events);
	IOServiceClose(conn);
	return 0;
}
