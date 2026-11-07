/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * FSEvents (Atlas.md, K0): a stream registers its paths with
 * fseventsd (fseventsd.defs) and gets an FSEventsMessage per event on a
 * port of its own, read by a thread of its own. The events wait for the
 * stream's latency (or go at once with kFSEventStreamCreateFlagNoDefer
 * when none waited), then go to the callback on the stream's dispatch
 * queue or run loop. Without kFSEventStreamCreateFlagFileEvents the paths
 * are the directories the items are in, with a trailing slash, once each.
 */

#include "FSEvents.h"
#include "FSEventsMessage.h"
#include "fseventsd.h"

#include <mach/mach.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
	char *path;
	FSEventStreamEventFlags flags;
	FSEventStreamEventId id;
} Event;

struct __FSEventStream {
	int refcount;
	FSEventStreamCallback callback;
	FSEventStreamContext context;
	CFArrayRef paths;
	FSEventStreamCreateFlags flags;
	CFTimeInterval latency;
	FSEventStreamEventId sinceWhen;
	FSEventStreamEventId latest;
	pthread_mutex_t lock;

	dispatch_queue_t queue;
	CFRunLoopRef runLoop;
	CFRunLoopSourceRef source;

	mach_port_t port;
	uint32_t stream;
	int started;
	int flushPending;	/* a delivery is scheduled */
	int deferred;		/* NoDefer: events wait for the latency now */
	Event *pending;
	size_t count, capacity;
	pthread_t reader;
};

static mach_port_t
server(void)
{
	static mach_port_t port = MACH_PORT_NULL;
	static pthread_mutex_t serverLock = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutex_lock(&serverLock);
	if (port != MACH_PORT_NULL) {
		mach_port_type_t type;
		if (mach_port_type(mach_task_self(), port, &type) != KERN_SUCCESS ||
		    (type & MACH_PORT_TYPE_DEAD_NAME)) {
			mach_port_deallocate(mach_task_self(), port);
			port = MACH_PORT_NULL;
		}
	}
	if (port == MACH_PORT_NULL &&
	    bootstrap_look_up(bootstrap_port, kFSEventsServiceName, &port) != KERN_SUCCESS)
		port = MACH_PORT_NULL;
	pthread_mutex_unlock(&serverLock);
	return port;
}

/* ---- Creating ---- */

FSEventStreamRef
FSEventStreamCreate(CFAllocatorRef allocator, FSEventStreamCallback callback,
    FSEventStreamContext *context, CFArrayRef pathsToWatch, FSEventStreamEventId sinceWhen,
    CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	(void)allocator;
	if (callback == NULL || pathsToWatch == NULL || CFArrayGetCount(pathsToWatch) == 0)
		return NULL;
	FSEventStreamRef s = calloc(1, sizeof(*s));
	if (s == NULL)
		return NULL;
	s->refcount = 1;
	s->callback = callback;
	if (context != NULL) {
		s->context = *context;
		if (s->context.retain != NULL && s->context.info != NULL)
			s->context.info = (void *)s->context.retain(s->context.info);
	}
	s->paths = CFArrayCreateCopy(NULL, pathsToWatch);
	s->flags = flags;
	s->latency = latency < 0 ? 0 : latency;
	s->sinceWhen = sinceWhen;
	s->latest = sinceWhen == kFSEventStreamEventIdSinceNow ? FSEventsGetCurrentEventId() : sinceWhen;
	pthread_mutex_init(&s->lock, NULL);
	return s;
}

/* SHORTCUT(K0-device): streams relative to a device are not kept. */
FSEventStreamRef
FSEventStreamCreateRelativeToDevice(CFAllocatorRef allocator, FSEventStreamCallback callback,
    FSEventStreamContext *context, dev_t deviceToWatch, CFArrayRef pathsToWatchRelativeToDevice,
    FSEventStreamEventId sinceWhen, CFTimeInterval latency, FSEventStreamCreateFlags flags)
{
	(void)allocator; (void)callback; (void)context; (void)deviceToWatch;
	(void)pathsToWatchRelativeToDevice; (void)sinceWhen; (void)latency; (void)flags;
	return NULL;
}

void
FSEventStreamRetain(FSEventStreamRef s)
{
	__atomic_add_fetch(&s->refcount, 1, __ATOMIC_SEQ_CST);
}

void
FSEventStreamRelease(FSEventStreamRef s)
{
	if (__atomic_sub_fetch(&s->refcount, 1, __ATOMIC_SEQ_CST) != 0)
		return;
	FSEventStreamStop(s);
	FSEventStreamInvalidate(s);
	for (size_t i = 0; i < s->count; i++)
		free(s->pending[i].path);
	free(s->pending);
	if (s->context.release != NULL && s->context.info != NULL)
		s->context.release(s->context.info);
	CFRelease(s->paths);
	pthread_mutex_destroy(&s->lock);
	free(s);
}

/* ---- Information ---- */

FSEventStreamEventId
FSEventStreamGetLatestEventId(ConstFSEventStreamRef s)
{
	return s->latest;
}

dev_t
FSEventStreamGetDeviceBeingWatched(ConstFSEventStreamRef s)
{
	(void)s;
	return 0;
}

CFArrayRef
FSEventStreamCopyPathsBeingWatched(ConstFSEventStreamRef s)
{
	return CFArrayCreateCopy(NULL, s->paths);
}

FSEventStreamEventId
FSEventsGetCurrentEventId(void)
{
	uint64_t current = 0;
	mach_port_t port = server();

	if (port == MACH_PORT_NULL || fsed_current_event_id(port, &current) != KERN_SUCCESS)
		return 0;
	return current;
}

CFUUIDRef
FSEventsCopyUUIDForDevice(dev_t dev)
{
	(void)dev;
	return NULL;	/* no per-volume history (SHORTCUT(K0-history)) */
}

FSEventStreamEventId
FSEventsGetLastEventIdForDeviceBeforeTime(dev_t dev, CFAbsoluteTime time)
{
	(void)dev; (void)time;
	return 0;
}

Boolean
FSEventsPurgeEventsForDeviceUpToEventId(dev_t dev, FSEventStreamEventId eventId)
{
	(void)dev; (void)eventId;
	return true;
}

CFStringRef
FSEventStreamCopyDescription(ConstFSEventStreamRef s)
{
	return CFStringCreateWithFormat(NULL, NULL,
	    CFSTR("FSEventStreamRef@%p:\n   flags = 0x%x\n   latency = %f\n   latestEventId = %llu\n   paths = %@"),
	    s, (unsigned)s->flags, s->latency, (unsigned long long)s->latest, s->paths);
}

void
FSEventStreamShow(ConstFSEventStreamRef s)
{
	CFStringRef d = FSEventStreamCopyDescription(s);
	CFShow(d);
	CFRelease(d);
}

Boolean
FSEventStreamSetExclusionPaths(FSEventStreamRef s, CFArrayRef pathsToExclude)
{
	(void)s; (void)pathsToExclude;
	return false;	/* SHORTCUT(K0): exclusions are not kept */
}

/* ---- Delivering ---- */

static void
deliver(FSEventStreamRef s)
{
	Event *events;
	size_t count;

	pthread_mutex_lock(&s->lock);
	events = s->pending;
	count = s->count;
	s->pending = NULL;
	s->count = s->capacity = 0;
	s->flushPending = 0;
	pthread_mutex_unlock(&s->lock);
	if (count == 0)
		return;

	FSEventStreamEventFlags *flags = malloc(sizeof(*flags) * count);
	FSEventStreamEventId *ids = malloc(sizeof(*ids) * count);
	void *paths;
	char **cPaths = NULL;
	CFMutableArrayRef cfPaths = NULL;

	if (s->flags & kFSEventStreamCreateFlagUseCFTypes) {
		cfPaths = CFArrayCreateMutable(NULL, (CFIndex)count, &kCFTypeArrayCallBacks);
		paths = cfPaths;
	} else {
		cPaths = malloc(sizeof(char *) * count);
		paths = cPaths;
	}
	for (size_t i = 0; i < count; i++) {
		flags[i] = events[i].flags;
		ids[i] = events[i].id;
		if (cfPaths != NULL) {
			CFStringRef path = CFStringCreateWithFileSystemRepresentation(NULL, events[i].path);
			if (s->flags & kFSEventStreamCreateFlagUseExtendedData) {
				const void *keys[] = { kFSEventStreamEventExtendedDataPathKey };
				const void *values[] = { path };
				CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 1,
				    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				CFArrayAppendValue(cfPaths, d);
				CFRelease(d);
			} else
				CFArrayAppendValue(cfPaths, path);
			CFRelease(path);
		} else
			cPaths[i] = events[i].path;
	}
	s->latest = ids[count - 1];
	s->callback(s, s->context.info, count, paths, flags, ids);

	for (size_t i = 0; i < count; i++)
		free(events[i].path);
	free(events);
	free(flags);
	free(ids);
	free(cPaths);
	if (cfPaths != NULL)
		CFRelease(cfPaths);
}

static void
performSource(void *info)
{
	deliver(info);
}

/* Delivery on the stream's queue or run loop; called with the lock held. */
static void
scheduleDelivery(FSEventStreamRef s, double delay)
{
	if (s->flushPending)
		return;
	s->flushPending = 1;
	if (s->queue != NULL) {
		FSEventStreamRetain(s);
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), s->queue, ^{
			deliver(s);
			FSEventStreamRelease(s);
		});
	} else if (s->runLoop != NULL) {
		CFRunLoopRef rl = s->runLoop;
		CFRunLoopSourceRef source = s->source;
		CFRetain(source);
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
		    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
			CFRunLoopSourceSignal(source);
			CFRunLoopWakeUp(rl);
			CFRelease(source);
		});
	}
}

/* Without FileEvents: the directory the item is in, with a trailing
   slash (the item itself when it is a watched directory's change). */
static char *
directoryOf(const char *path)
{
	size_t n;
	char *dir;
	const char *slash = strrchr(path, '/');
	if (slash == NULL || slash == path)
		return strdup("/");
	n = (size_t)(slash - path) + 1;
	dir = malloc(n + 1);
	memcpy(dir, path, n);
	dir[n] = '\0';
	return dir;
}

static void
addEvent(FSEventStreamRef s, const FSEventsMessage *m)
{
	char *path;
	FSEventStreamEventFlags flags = m->flags;
	const FSEventStreamEventFlags system = kFSEventStreamEventFlagMustScanSubDirs |
	    kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped |
	    kFSEventStreamEventFlagHistoryDone | kFSEventStreamEventFlagOwnEvent;

	if (s->flags & kFSEventStreamCreateFlagFileEvents)
		path = strdup(m->path);
	else {
		path = (flags & kFSEventStreamEventFlagMustScanSubDirs) ? strdup(m->path) : directoryOf(m->path);
		if (path[strlen(path) - 1] != '/') {
			char *slashed = malloc(strlen(path) + 2);
			sprintf(slashed, "%s/", path);
			free(path);
			path = slashed;
		}
		flags &= system;
	}

	pthread_mutex_lock(&s->lock);
	/* A directory already waiting: once, with the newest ID. */
	if (!(s->flags & kFSEventStreamCreateFlagFileEvents)) {
		for (size_t i = 0; i < s->count; i++)
			if (strcmp(s->pending[i].path, path) == 0) {
				s->pending[i].flags |= flags;
				s->pending[i].id = m->id;
				free(path);
				pthread_mutex_unlock(&s->lock);
				return;
			}
	}
	if (s->count == s->capacity) {
		s->capacity = s->capacity ? s->capacity * 2 : 16;
		s->pending = realloc(s->pending, sizeof(Event) * s->capacity);
	}
	s->pending[s->count++] = (Event){ path, flags, m->id };
	if (!s->flushPending) {
		/* NoDefer: the first event goes now, the next ones wait. */
		if ((s->flags & kFSEventStreamCreateFlagNoDefer) && !s->deferred) {
			s->deferred = 1;
			scheduleDelivery(s, 0);
			FSEventStreamRetain(s);
			dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(s->latency * NSEC_PER_SEC)),
			    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
				pthread_mutex_lock(&s->lock);
				s->deferred = 0;
				pthread_mutex_unlock(&s->lock);
				FSEventStreamRelease(s);
			});
		} else
			scheduleDelivery(s, s->latency);
	}
	pthread_mutex_unlock(&s->lock);
}

static void *
readEvents(void *arg)
{
	FSEventStreamRef s = arg;
	mach_port_t port = s->port;

	for (;;) {
		struct {
			FSEventsMessage m;
			mach_msg_trailer_t trailer;
		} msg;
		if (mach_msg(&msg.m.header, MACH_RCV_MSG, 0, sizeof(msg), port,
		    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
			break;		/* the port went: stopped */
		if (msg.m.header.msgh_id != kFSEventsMessageID)
			continue;
		msg.m.path[sizeof(msg.m.path) - 1] = '\0';
		addEvent(s, &msg.m);
	}
	return NULL;
}

/* ---- Scheduling ---- */

void
FSEventStreamScheduleWithRunLoop(FSEventStreamRef s, CFRunLoopRef runLoop, CFStringRef mode)
{
	CFRunLoopSourceContext context = { 0, s, NULL, NULL, NULL, NULL, NULL, NULL, NULL, performSource };

	if (s->source == NULL)
		s->source = CFRunLoopSourceCreate(NULL, 0, &context);
	s->runLoop = runLoop;
	CFRunLoopAddSource(runLoop, s->source, mode);
}

void
FSEventStreamUnscheduleFromRunLoop(FSEventStreamRef s, CFRunLoopRef runLoop, CFStringRef mode)
{
	if (s->source != NULL)
		CFRunLoopRemoveSource(runLoop, s->source, mode);
}

void
FSEventStreamSetDispatchQueue(FSEventStreamRef s, dispatch_queue_t queue)
{
	if (s->queue != NULL)
		dispatch_release(s->queue);
	s->queue = queue;
	if (queue != NULL)
		dispatch_retain(queue);
}

void
FSEventStreamInvalidate(FSEventStreamRef s)
{
	if (s->source != NULL) {
		CFRunLoopSourceInvalidate(s->source);
		CFRelease(s->source);
		s->source = NULL;
	}
	s->runLoop = NULL;
	FSEventStreamSetDispatchQueue(s, NULL);
}

/* ---- Running ---- */

Boolean
FSEventStreamStart(FSEventStreamRef s)
{
	mach_port_t srv = server();
	uint64_t current = 0;
	char *paths = NULL;
	size_t length = 0;

	if (s->started)
		return true;
	if (srv == MACH_PORT_NULL || (s->queue == NULL && s->runLoop == NULL))
		return false;

	/* The watched paths, absolute and resolved, NUL-separated. */
	for (CFIndex i = 0; i < CFArrayGetCount(s->paths); i++) {
		char raw[PATH_MAX], resolved[PATH_MAX];
		if (!CFStringGetFileSystemRepresentation(CFArrayGetValueAtIndex(s->paths, i), raw, sizeof(raw)))
			continue;
		const char *p = realpath(raw, resolved) ? resolved : raw;
		size_t n = strlen(p) + 1;
		paths = realloc(paths, length + n);
		memcpy(paths + length, p, n);
		length += n;
	}
	if (length == 0)
		return false;

	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->port) != KERN_SUCCESS) {
		free(paths);
		return false;
	}
	/* A deep queue: events come in bursts. */
	mach_port_limits_t limits = { .mpl_qlimit = MACH_PORT_QLIMIT_LARGE };
	mach_port_set_attributes(mach_task_self(), s->port, MACH_PORT_LIMITS_INFO,
	    (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);
	kern_return_t kr = fsed_register_stream(srv, s->port, s->flags, paths,
	    (mach_msg_type_number_t)length, &s->stream, &current);
	free(paths);
	if (kr != KERN_SUCCESS || s->stream == 0) {
		mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_RECEIVE, -1);
		s->port = MACH_PORT_NULL;
		return false;
	}
	s->started = 1;
	FSEventStreamRetain(s);
	pthread_create(&s->reader, NULL, readEvents, s);

	/* SHORTCUT(K0-history): nothing is replayed; history is done at once. */
	if (s->sinceWhen != kFSEventStreamEventIdSinceNow) {
		pthread_mutex_lock(&s->lock);
		if (s->count == s->capacity) {
			s->capacity = s->capacity ? s->capacity * 2 : 16;
			s->pending = realloc(s->pending, sizeof(Event) * s->capacity);
		}
		s->pending[s->count++] = (Event){ strdup(""), kFSEventStreamEventFlagHistoryDone, current };
		scheduleDelivery(s, 0);
		pthread_mutex_unlock(&s->lock);
	}
	return true;
}

FSEventStreamEventId
FSEventStreamFlushAsync(FSEventStreamRef s)
{
	pthread_mutex_lock(&s->lock);
	FSEventStreamEventId id = s->count ? s->pending[s->count - 1].id : s->latest;
	if (s->count)
		s->flushPending = 0, scheduleDelivery(s, 0);
	pthread_mutex_unlock(&s->lock);
	return id;
}

void
FSEventStreamFlushSync(FSEventStreamRef s)
{
	deliver(s);
}

void
FSEventStreamStop(FSEventStreamRef s)
{
	mach_port_t srv;

	if (!s->started)
		return;
	s->started = 0;
	srv = server();
	if (srv != MACH_PORT_NULL)
		fsed_unregister_stream(srv, s->stream);
	/* The reader ends when its port goes. */
	mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_RECEIVE, -1);
	s->port = MACH_PORT_NULL;
	pthread_join(s->reader, NULL);
	FSEventStreamRelease(s);
}
