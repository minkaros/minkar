/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * One message as logd keeps it, on disk and in query replies: this header,
 * then the process name, subsystem, category and text, each ended by a
 * NUL; the whole padded to 8 bytes (size).
 */

#ifndef _LOGD_RECORD_H_
#define _LOGD_RECORD_H_

#include <stdint.h>

typedef uint8_t *logd_bytes_t;
typedef char logd_name_t[256];

/* Types, as os_log_type_t's: */
#define LOGD_TYPE_DEFAULT	0x00
#define LOGD_TYPE_INFO		0x01
#define LOGD_TYPE_DEBUG		0x02
#define LOGD_TYPE_ERROR		0x10
#define LOGD_TYPE_FAULT		0x11
/* and their bits in a query's mask. */
#define LOGD_MASK(type)		(1u << ((type) & 0x1f))

typedef struct {
	uint32_t	size;
	uint32_t	type;
	uint64_t	seq;
	uint64_t	time;		/* CLOCK_REALTIME, nanoseconds */
	uint64_t	thread;
	uint64_t	activity;
	int32_t		pid;
	uint32_t	uid;
	uint16_t	process_len;	/* each with its NUL */
	uint16_t	subsystem_len;
	uint16_t	category_len;
	uint16_t	text_len;
} logd_record_t;

static inline const char *logd_record_process(const logd_record_t *r)
{ return ((const char *)(r + 1)); }
static inline const char *logd_record_subsystem(const logd_record_t *r)
{ return (logd_record_process(r) + r->process_len); }
static inline const char *logd_record_category(const logd_record_t *r)
{ return (logd_record_subsystem(r) + r->subsystem_len); }
static inline const char *logd_record_text(const logd_record_t *r)
{ return (logd_record_category(r) + r->category_len); }

#endif /* _LOGD_RECORD_H_ */
