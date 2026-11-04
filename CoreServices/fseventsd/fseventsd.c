/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * fseventsd (Dock-Argos.md, K0): the file system's events for FSEvents.
 * It reads the kernel's events from /dev/fsevents (a clone of it, every
 * event type), gives each an ID (they keep growing across restarts:
 * /var/db/fseventsd/last-id), and sends each stream registered through
 * fseventsd.defs the events under the paths it watches, one Mach message
 * (FSEventsMessage.h) per event. A stream whose port does not take a
 * message (its queue is full) gets the next one marked
 * MustScanSubDirs|UserDropped on its root; one whose port dies is
 * forgotten.
 *
 * SHORTCUT(K0-history): no history is kept on the volumes, so a stream
 * asking for events since an ID gets only HistoryDone, then what comes.
 */

#include <bsm/libbsm.h>
#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsevents.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FSEvents.h"
#include "FSEventsMessage.h"
#include "fseventsdServer.h"

#define LAST_ID_FILE	"/var/db/fseventsd/last-id"
#define ID_SAVE_EVERY	256
#define MAX_STREAMS	256

typedef struct {
	uint32_t id;		/* 0: free */
	mach_port_t port;
	pid_t pid;
	uint32_t flags;
	int dropped;		/* a message was not taken */
	int count;
	char **paths;		/* without a trailing slash; "/" for the root */
} Stream;

static Stream streams[MAX_STREAMS];
static uint32_t nextStream = 1;
static uint64_t currentID;
static uint64_t savedID;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t notifyPort;

/* ---- Event IDs ---- */

static void
saveID(uint64_t id)
{
	FILE *f = fopen(LAST_ID_FILE ".new", "w");

	if (f == NULL)
		return;
	fprintf(f, "%llu\n", (unsigned long long)id);
	fclose(f);
	rename(LAST_ID_FILE ".new", LAST_ID_FILE);
	savedID = id;
}

/* Starts past the last ID saved and the ones that may have been given
 * since (saved every ID_SAVE_EVERY). */
static void
loadID(void)
{
	FILE *f = fopen(LAST_ID_FILE, "r");
	unsigned long long id = 0;

	mkdir("/var/db/fseventsd", 0755);
	if (f != NULL) {
		if (fscanf(f, "%llu", &id) != 1)
			id = 0;
		fclose(f);
	}
	currentID = id + ID_SAVE_EVERY + 1;
	saveID(currentID);
}

/* ---- Streams ---- */

static void
freeStream(Stream *s)
{
	for (int i = 0; i < s->count; i++)
		free(s->paths[i]);
	free(s->paths);
	if (s->port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), s->port);
	memset(s, 0, sizeof(*s));
}

/* Whether path is a watched path or under it. */
static int
watches(const Stream *s, const char *path)
{
	for (int i = 0; i < s->count; i++) {
		const char *w = s->paths[i];
		size_t n = strlen(w);
		if (strcmp(w, "/") == 0)
			return 1;
		if (strncmp(path, w, n) == 0 && (path[n] == '\0' || path[n] == '/'))
			return 1;
	}
	return 0;
}

static int
sendEvent(Stream *s, uint64_t id, uint32_t flags, const char *path)
{
	FSEventsMessage msg;

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = s->port;
	msg.header.msgh_id = kFSEventsMessageID;
	msg.id = id;
	msg.flags = flags;
	msg.stream = s->id;
	strlcpy(msg.path, path, sizeof(msg.path));
	return mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0,
	    MACH_PORT_NULL, 0, MACH_PORT_NULL);
}

/* One event to every stream that watches its path; called with lock held. */
static void
dispatchEvent(uint64_t id, uint32_t flags, pid_t pid, const char *path)
{
	for (int i = 0; i < MAX_STREAMS; i++) {
		Stream *s = &streams[i];
		uint32_t f = flags;

		if (s->id == 0 || !watches(s, path))
			continue;
		if (pid == s->pid) {
			if (s->flags & kFSEventsRegisterIgnoreSelf)
				continue;
			if (s->flags & kFSEventsRegisterMarkSelf)
				f |= kFSEventStreamEventFlagOwnEvent;
		}
		if (s->dropped && sendEvent(s, id, kFSEventStreamEventFlagMustScanSubDirs |
		    kFSEventStreamEventFlagUserDropped, s->paths[0]) == MACH_MSG_SUCCESS)
			s->dropped = 0;
		kern_return_t kr = sendEvent(s, id, f, path);
		if (kr == MACH_SEND_TIMED_OUT || kr == MACH_SEND_NO_BUFFER)
			s->dropped = 1;
		else if (kr == MACH_SEND_INVALID_DEST)
			freeStream(s);
	}
}

/* ---- The kernel's events ---- */

static uint32_t
itemFlags(int32_t type, int32_t mode)
{
	uint32_t flags = 0;

	switch (type) {
	case FSE_CREATE_FILE:
	case FSE_CREATE_DIR:
		flags = kFSEventStreamEventFlagItemCreated;
		break;
	case FSE_DELETE:
		flags = kFSEventStreamEventFlagItemRemoved;
		break;
	case FSE_RENAME:
	case FSE_EXCHANGE:
		flags = kFSEventStreamEventFlagItemRenamed;
		break;
	case FSE_CONTENT_MODIFIED:
		flags = kFSEventStreamEventFlagItemModified;
		break;
	case FSE_STAT_CHANGED:
		flags = kFSEventStreamEventFlagItemInodeMetaMod;
		break;
	case FSE_FINDER_INFO_CHANGED:
		flags = kFSEventStreamEventFlagItemFinderInfoMod;
		break;
	case FSE_CHOWN:
		flags = kFSEventStreamEventFlagItemChangeOwner;
		break;
	case FSE_XATTR_MODIFIED:
	case FSE_XATTR_REMOVED:
		flags = kFSEventStreamEventFlagItemXattrMod;
		break;
	case FSE_CLONE:
		flags = kFSEventStreamEventFlagItemCloned;
		break;
	}
	if (type == FSE_CREATE_DIR || S_ISDIR(mode))
		flags |= kFSEventStreamEventFlagItemIsDir;
	else if (S_ISLNK(mode))
		flags |= kFSEventStreamEventFlagItemIsSymlink;
	else if (mode != 0 || type == FSE_CREATE_FILE)
		flags |= kFSEventStreamEventFlagItemIsFile;
	if (mode & FSE_MODE_HLINK)
		flags |= kFSEventStreamEventFlagItemIsHardlink;
	if (mode & FSE_MODE_LAST_HLINK)
		flags |= kFSEventStreamEventFlagItemIsLastHardlink;
	return flags;
}

/* An event's records: type, pid, then (argument type, length, data)
 * until FSE_ARG_DONE. A rename has two paths, each with its own file
 * information. Returns the bytes used, or 0 for an incomplete event. */
static size_t
parseEvent(const uint8_t *buf, size_t len)
{
	size_t off = 0;
	int32_t rawType, type, pid;
	char paths[2][PATH_MAX];
	int32_t modes[2] = { 0, 0 };
	int npaths = 0;

	if (len < 8)
		return 0;
	memcpy(&rawType, buf, 4);
	memcpy(&pid, buf + 4, 4);
	off = 8;
	type = rawType & 0xfff;
	for (;;) {
		uint16_t argType, argLen;

		if (off + 2 > len)
			return 0;
		memcpy(&argType, buf + off, 2);
		off += 2;
		if (argType == FSE_ARG_DONE)
			break;
		if (off + 2 > len)
			return 0;
		memcpy(&argLen, buf + off, 2);
		off += 2;
		if (off + argLen > len)
			return 0;
		if (argType == FSE_ARG_STRING && npaths < 2) {
			size_t n = argLen < PATH_MAX ? argLen : PATH_MAX - 1;
			memcpy(paths[npaths], buf + off, n);
			paths[npaths][n] = '\0';
			npaths++;
		} else if (argType == FSE_ARG_MODE && argLen == 4 && npaths > 0) {
			memcpy(&modes[npaths - 1], buf + off, 4);
		}
		off += argLen;
	}

	if (type < 0 || type >= FSE_MAX_EVENTS || npaths == 0 ||
	    type == FSE_DOCID_CREATED || type == FSE_DOCID_CHANGED || type == FSE_UNMOUNT_PENDING)
		return off;

	pthread_mutex_lock(&lock);
	uint32_t dropped = (rawType >> FSE_FLAG_SHIFT) & FSE_CONTAINS_DROPPED_EVENTS ?
	    kFSEventStreamEventFlagKernelDropped | kFSEventStreamEventFlagMustScanSubDirs : 0;
	/* A rename's new name that did not exist has no file information:
	 * it is the same item as the old name. */
	if (npaths == 2 && modes[1] == 0)
		modes[1] = modes[0];
	for (int i = 0; i < npaths; i++) {
		uint64_t id = ++currentID;
		dispatchEvent(id, itemFlags(type, modes[i]) | dropped, pid, paths[i]);
	}
	if (currentID - savedID >= ID_SAVE_EVERY)
		saveID(currentID);
	pthread_mutex_unlock(&lock);
	return off;
}

static void *
readEvents(void *unused)
{
	int8_t list[FSE_MAX_EVENTS];
	int devfd, fd = -1;
	fsevent_clone_args args;
	static uint8_t buf[256 * 1024];
	size_t have = 0;

	(void)unused;
	for (int i = 0; i < FSE_MAX_EVENTS; i++)
		list[i] = FSE_REPORT;
	devfd = open("/dev/fsevents", O_RDONLY);
	if (devfd < 0) {
		fprintf(stderr, "fseventsd: /dev/fsevents: %s\n", strerror(errno));
		exit(1);
	}
	memset(&args, 0, sizeof(args));
	args.event_list = list;
	args.num_events = FSE_MAX_EVENTS;
	args.event_queue_depth = 4096;
	args.fd = &fd;
	if (ioctl(devfd, FSEVENTS_CLONE, &args) < 0) {
		fprintf(stderr, "fseventsd: FSEVENTS_CLONE: %s\n", strerror(errno));
		exit(1);
	}
	close(devfd);
	if (ioctl(fd, FSEVENTS_WANT_EXTENDED_INFO, NULL) < 0)
		fprintf(stderr, "fseventsd: FSEVENTS_WANT_EXTENDED_INFO: %s\n", strerror(errno));

	for (;;) {
		ssize_t n = read(fd, buf + have, sizeof(buf) - have);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			fprintf(stderr, "fseventsd: read: %s\n", n < 0 ? strerror(errno) : "end");
			exit(1);
		}
		have += (size_t)n;
		size_t off = 0, used;
		while ((used = parseEvent(buf + off, have - off)) > 0)
			off += used;
		memmove(buf, buf + off, have - off);
		have -= off;
	}
	return NULL;
}

/* ---- The MIG interface ---- */

kern_return_t
fsedsrv_register_stream(mach_port_t server, audit_token_t token, mach_port_t port,
    uint32_t flags, char *paths, mach_msg_type_number_t pathsCount, uint32_t *stream,
    uint64_t *current)
{
	(void)server;
	*stream = 0;
	pthread_mutex_lock(&lock);
	*current = currentID;
	for (int i = 0; i < MAX_STREAMS; i++) {
		Stream *s = &streams[i];
		if (s->id != 0)
			continue;
		s->id = nextStream++;
		s->port = port;
		s->pid = audit_token_to_pid(token);
		s->flags = flags;
		/* NUL-separated paths, without trailing slashes. */
		for (mach_msg_type_number_t off = 0; off < pathsCount; ) {
			const char *p = paths + off;
			size_t n = strnlen(p, pathsCount - off);
			if (n > 0) {
				char *copy = strndup(p, n);
				while (strlen(copy) > 1 && copy[strlen(copy) - 1] == '/')
					copy[strlen(copy) - 1] = '\0';
				s->paths = realloc(s->paths, sizeof(char *) * (s->count + 1));
				s->paths[s->count++] = copy;
			}
			off += n + 1;
		}
		if (s->count == 0) {
			freeStream(s);
			break;
		}
		mach_port_t previous = MACH_PORT_NULL;
		mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 1,
		    notifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
		if (previous != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), previous);
		*stream = s->id;
		pthread_mutex_unlock(&lock);
		return KERN_SUCCESS;
	}
	pthread_mutex_unlock(&lock);
	mach_port_deallocate(mach_task_self(), port);
	return KERN_SUCCESS;
}

kern_return_t
fsedsrv_unregister_stream(mach_port_t server, audit_token_t token, uint32_t stream)
{
	(void)server;
	pthread_mutex_lock(&lock);
	for (int i = 0; i < MAX_STREAMS; i++)
		if (streams[i].id == stream && streams[i].pid == audit_token_to_pid(token))
			freeStream(&streams[i]);
	pthread_mutex_unlock(&lock);
	return KERN_SUCCESS;
}

kern_return_t
fsedsrv_current_event_id(mach_port_t server, uint64_t *current)
{
	(void)server;
	pthread_mutex_lock(&lock);
	*current = currentID;
	pthread_mutex_unlock(&lock);
	return KERN_SUCCESS;
}

/* A stream's port died: forget the stream. */
static void *
watchDeaths(void *unused)
{
	(void)unused;
	for (;;) {
		struct {
			mach_dead_name_notification_t n;
			mach_msg_trailer_t trailer;
		} msg;
		if (mach_msg(&msg.n.not_header, MACH_RCV_MSG, 0, sizeof(msg), notifyPort,
		    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
			continue;
		if (msg.n.not_header.msgh_id != MACH_NOTIFY_DEAD_NAME)
			continue;
		pthread_mutex_lock(&lock);
		for (int i = 0; i < MAX_STREAMS; i++)
			if (streams[i].id != 0 && streams[i].port == msg.n.not_port)
				freeStream(&streams[i]);
		pthread_mutex_unlock(&lock);
		/* The dead name the notification carried. */
		mach_port_deallocate(mach_task_self(), msg.n.not_port);
	}
	return NULL;
}

int
main(void)
{
	mach_port_t service;
	pthread_t reader, deaths;

	setvbuf(stderr, NULL, _IOLBF, 0);
	if (bootstrap_check_in(bootstrap_port, kFSEventsServiceName, &service) != KERN_SUCCESS) {
		fprintf(stderr, "fseventsd: cannot check in %s\n", kFSEventsServiceName);
		return 1;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) != KERN_SUCCESS)
		return 1;
	loadID();
	pthread_create(&deaths, NULL, watchDeaths, NULL);
	pthread_create(&reader, NULL, readEvents, NULL);
	fprintf(stderr, "fseventsd: serving %s from event %llu\n", kFSEventsServiceName,
	    (unsigned long long)currentID);
	mach_msg_server(fseventsd_server, 32768 + 512, service,
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
	    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	return 1;
}
