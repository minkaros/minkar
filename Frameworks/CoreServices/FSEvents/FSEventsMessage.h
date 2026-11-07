/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What fseventsd sends a stream's port: one file system event, its ID,
 * its item flags (FSEvents.h's kFSEventStreamEventFlag*) and its path.
 * Private to FSEvents and fseventsd.
 */

#ifndef FSEVENTS_MESSAGE_H
#define FSEVENTS_MESSAGE_H

#include <mach/message.h>
#include <stdint.h>
#include <sys/syslimits.h>

#define kFSEventsServiceName	"org.minkar.fseventsd"
#define kFSEventsMessageID	0x46534576	/* 'FSEv' */

typedef struct {
	mach_msg_header_t header;
	uint64_t id;
	uint32_t flags;
	uint32_t stream;
	char path[PATH_MAX];
} FSEventsMessage;

/* The flags a stream registers with, as FSEventStreamCreate's, which
   fseventsd uses itself. */
#define kFSEventsRegisterIgnoreSelf	0x00000008
#define kFSEventsRegisterMarkSelf	0x00000020

#endif
