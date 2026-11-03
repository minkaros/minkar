/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The messages libsystem_trace sends logd (org.minkar.logd), written by
 * hand: libsystem_trace is built before MIG's users and liblaunch.
 *
 * LOGD_MSG_LOG: one os_log message, sent without waiting (a full queue
 * drops it). Who sent it, logd learns from the audit trailer; the name
 * it gives is only shown when the process has exited by then.
 * LOGD_MSG_LEVEL: which types a subsystem keeps beyond the default ones
 * (LOGD_LEVEL_*); the reply is a logd_level_reply_t.
 * logd's other requests (log show, log stream, log config) are in its
 * MIG interface (BSD/usr.sbin/logd/logd.defs).
 */

#ifndef _LOGD_MESSAGE_H_
#define _LOGD_MESSAGE_H_

#include <mach/message.h>
#include <stdint.h>

#define LOGD_SERVICE		"org.minkar.logd"
#define LOGD_MSG_LOG		0x6c6f6700	/* 'log\0' */
#define LOGD_MSG_LEVEL		0x6c6f6701
#define LOGD_MSG_LEVEL_REPLY	(LOGD_MSG_LEVEL + 100)

#define LOGD_NAME_MAX		256	/* subsystem, category: with their NUL */
#define LOGD_TEXT_MAX		4096	/* the message: with its NUL */

/* What a subsystem keeps: the default types always, then info, debug. */
#define LOGD_LEVEL_DEFAULT	0
#define LOGD_LEVEL_INFO		1
#define LOGD_LEVEL_DEBUG	2

typedef struct {
	mach_msg_header_t	header;
	uint64_t		time;		/* CLOCK_REALTIME, nanoseconds */
	uint64_t		thread;
	uint64_t		activity;
	uint32_t		type;		/* os_log_type_t */
	uint16_t		subsystem_len;	/* each with its NUL, in data */
	uint16_t		category_len;
	uint16_t		text_len;
	uint16_t		process_len;	/* the sender's name, shown when
						   it has exited by then */
	char			data[LOGD_NAME_MAX * 3 + LOGD_TEXT_MAX];
} logd_log_message_t;

typedef struct {
	mach_msg_header_t	header;
	char			subsystem[LOGD_NAME_MAX];
} logd_level_request_t;

typedef struct {
	mach_msg_header_t	header;
	int32_t			level;
} logd_level_reply_t;

#endif /* _LOGD_MESSAGE_H_ */
