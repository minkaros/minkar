/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * os_log's way to logd: the port looked up once (again after logd
 * restarts), messages sent without waiting, and each subsystem's level
 * asked of logd and kept for a few seconds.
 */

#include <dlfcn.h>
#include <stddef.h>
#include <mach/mach.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <os/log.h>
#include "logd_message.h"
#include "logd_client.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t logd = MACH_PORT_NULL;
static uint64_t retry_after;	/* no logd: when to look again */

static uint64_t
now_ns(void)
{
	return (clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW));
}

/* liblaunch's bootstrap_look_up, found at run time: liblaunch is built
 * after this library. */
static mach_port_t
lookup(void)
{
	static kern_return_t (*look_up)(mach_port_t, const char *, mach_port_t *);
	mach_port_t port = MACH_PORT_NULL;

	if (look_up == NULL)
		look_up = (kern_return_t (*)(mach_port_t, const char *, mach_port_t *))
		    dlsym(RTLD_DEFAULT, "bootstrap_look_up");
	if (look_up == NULL || bootstrap_port == MACH_PORT_NULL)
		return (MACH_PORT_NULL);
	if (look_up(bootstrap_port, LOGD_SERVICE, &port) != KERN_SUCCESS)
		return (MACH_PORT_NULL);
	return (port);
}

/* A send right to logd, or null (then for a second, none is looked up). */
static mach_port_t
logd_port(void)
{
	mach_port_t port;

	pthread_mutex_lock(&lock);
	if (logd == MACH_PORT_NULL && now_ns() >= retry_after) {
		logd = lookup();
		if (logd == MACH_PORT_NULL)
			retry_after = now_ns() + 1000000000ULL;
	}
	port = logd;
	pthread_mutex_unlock(&lock);
	return (port);
}

/* logd went away (MACH_SEND_INVALID_DEST): look it up again next time. */
static void
logd_lost(mach_port_t port)
{
	pthread_mutex_lock(&lock);
	if (logd == port) {
		mach_port_deallocate(mach_task_self(), port);
		logd = MACH_PORT_NULL;
		retry_after = 0;
	}
	pthread_mutex_unlock(&lock);
}

static uint16_t
put(char *dst, size_t room, const char *s)
{
	size_t n = s != NULL ? strnlen(s, room - 1) : 0;

	if (n > 0)
		memcpy(dst, s, n);
	dst[n] = '\0';
	return ((uint16_t)(n + 1));
}

bool
_logd_send(const char *subsystem, const char *category, os_log_type_t type,
    uint64_t activity, const char *text)
{
	mach_port_t port = logd_port();
	logd_log_message_t m;
	size_t size;
	kern_return_t kr;

	if (port == MACH_PORT_NULL)
		return (false);
	m.time = clock_gettime_nsec_np(CLOCK_REALTIME);
	pthread_threadid_np(NULL, &m.thread);
	m.activity = activity;
	m.type = type;
	m.subsystem_len = put(m.data, LOGD_NAME_MAX, subsystem);
	m.category_len = put(m.data + m.subsystem_len, LOGD_NAME_MAX, category);
	m.text_len = put(m.data + m.subsystem_len + m.category_len, LOGD_TEXT_MAX, text);
	m.process_len = put(m.data + m.subsystem_len + m.category_len + m.text_len, LOGD_NAME_MAX,
	    getprogname());
	size = offsetof(logd_log_message_t, data) + m.subsystem_len + m.category_len + m.text_len +
	    m.process_len;
	size = (size + 3) & ~(size_t)3;
	m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	m.header.msgh_size = (mach_msg_size_t)size;
	m.header.msgh_remote_port = port;
	m.header.msgh_local_port = MACH_PORT_NULL;
	m.header.msgh_voucher_port = MACH_PORT_NULL;
	m.header.msgh_id = LOGD_MSG_LOG;
	kr = mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, (mach_msg_size_t)size, 0,
	    MACH_PORT_NULL, 0, MACH_PORT_NULL);
	if (kr == MACH_SEND_INVALID_DEST) {
		logd_lost(port);
		return (false);
	}
	/* A full queue (MACH_SEND_TIMED_OUT) drops the message: logging
	 * never waits. It was meant for logd all the same. */
	return (kr == MACH_MSG_SUCCESS || kr == MACH_SEND_TIMED_OUT);
}

int
_logd_level(const char *subsystem)
{
	mach_port_t port = logd_port();
	union {
		logd_level_request_t request;
		struct {
			logd_level_reply_t reply;
			mach_msg_trailer_t trailer;
		} r;
	} m;
	mach_port_t reply;
	kern_return_t kr;
	int level = -1;

	if (port == MACH_PORT_NULL)
		return (-1);
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply) != KERN_SUCCESS)
		return (-1);
	memset(&m, 0, sizeof(m));
	put(m.request.subsystem, LOGD_NAME_MAX, subsystem);
	m.request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE);
	m.request.header.msgh_size = sizeof(m.request);
	m.request.header.msgh_remote_port = port;
	m.request.header.msgh_local_port = reply;
	m.request.header.msgh_id = LOGD_MSG_LEVEL;
	/* At most 100 ms: a busy logd makes the subsystem keep the default. */
	kr = mach_msg(&m.request.header, MACH_SEND_MSG | MACH_RCV_MSG | MACH_SEND_TIMEOUT |
	    MACH_RCV_TIMEOUT, sizeof(m.request), sizeof(m.r), reply, 100, MACH_PORT_NULL);
	if (kr == MACH_MSG_SUCCESS && m.r.reply.header.msgh_id == LOGD_MSG_LEVEL_REPLY &&
	    m.r.reply.header.msgh_size >= sizeof(m.r.reply))
		level = m.r.reply.level;
	else if (kr == MACH_SEND_INVALID_DEST)
		logd_lost(port);
	mach_port_mod_refs(mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1);
	return (level);
}

/* Subsystems' levels, kept for a few seconds: log config reaches running
 * processes soon, without asking logd at every message. */
#define LEVEL_CACHE	16
#define LEVEL_TTL	(5ULL * 1000000000ULL)

static struct {
	char		name[64];
	int		level;
	uint64_t	expires;
} levels[LEVEL_CACHE];
static unsigned next_slot;

static int
cached_level(const char *subsystem)
{
	const char *name = subsystem != NULL ? subsystem : "";
	uint64_t now = now_ns();
	int level;

	pthread_mutex_lock(&lock);
	for (unsigned i = 0; i < LEVEL_CACHE; i++)
		if (levels[i].expires > now && strncmp(levels[i].name, name,
		    sizeof(levels[i].name)) == 0) {
			level = levels[i].level;
			pthread_mutex_unlock(&lock);
			return (level);
		}
	pthread_mutex_unlock(&lock);
	level = _logd_level(name);
	if (level < 0)
		level = LOGD_LEVEL_INFO;	/* no logd: info by default */
	pthread_mutex_lock(&lock);
	unsigned slot = next_slot++ % LEVEL_CACHE;
	strlcpy(levels[slot].name, name, sizeof(levels[slot].name));
	levels[slot].level = level;
	levels[slot].expires = now + LEVEL_TTL;
	pthread_mutex_unlock(&lock);
	return (level);
}

bool
_logd_keeps(const char *subsystem, os_log_type_t type)
{
	if (type == OS_LOG_TYPE_INFO)
		return (cached_level(subsystem) >= LOGD_LEVEL_INFO);
	if (type == OS_LOG_TYPE_DEBUG)
		return (cached_level(subsystem) >= LOGD_LEVEL_DEBUG);
	return (true);
}
