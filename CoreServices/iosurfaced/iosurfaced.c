/*
 * SPDX-License-Identifier: BSD-2-Clause
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
 * iosurfaced: the IOSurface registry (roadmap step 4). It keeps each
 * surface's memory entry, properties and values under a global ID, and
 * which processes hold it; a process's holds go when it dies (its session
 * port goes dead), and a surface no process holds goes with them.
 *
 * A surface's ID is no secret (IDs are handed out in turn), so the ID alone
 * gives nothing: a process finds a surface by ID only if it holds it
 * already, if it was given the surface's port (IOSurfaceCreateMachPort,
 * redeemed once by port_id), if the surface is global (kIOSurfaceIsGlobal),
 * or if it is privileged: root, or a program with the gpu-client
 * entitlement (the window server, which shows apps' surfaces). The same
 * goes for a surface's values, its port and its GPU buffer.
 * Started on demand by launchd (org.minkar.iosurfaced).
 */

#include <arpa/inet.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "IOSurfaceRegistryServer.h"	/* MIG, generated */
#include "MinkarAMDGPUInterface.h"

#define kService	"org.minkar.iosurfaced"

/* sys/codesign.h (private): the kernel's copy of a process's entitlements. */
#define CS_OPS_ENTITLEMENTS_BLOB	7
int csops_audittoken(pid_t pid, unsigned int ops, void *useraddr, size_t usersize,
    audit_token_t *token);

typedef struct Hold {
	pid_t		pid;
	uint32_t	count;
	struct Hold	*next;
} Hold;

typedef struct Surface {
	uint32_t	surfaceID;
	mach_port_t	memory;		/* send right to the memory entry */
	uint64_t	allocSize;
	void		*properties;
	size_t		propertiesLen;
	void		*values;
	size_t		valuesLen;
	mach_port_t	port;		/* receive right, made_port's; or null */
	Hold		*holds;
	Hold		*grants;	/* given its port: may look it up once */
	Boolean		global;		/* kIOSurfaceIsGlobal: anyone finds it */
	io_connect_t	gpu;		/* a surface in GPU memory: its GPU, */
	uint64_t	gpuBuffer;	/* and the buffer held there; */
	uint64_t	gpuEntryID;	/* the GPU's registry entry, */
	uint64_t	gpuName;	/* and the buffer's name */
	struct Surface	*next;
} Surface;

typedef struct Session {
	pid_t		pid;
	mach_port_t	port;		/* send right, watched for death */
	struct Session	*next;
} Session;

static Surface *surfaces;
static Session *sessions;
static uint32_t nextID = 1;
static mach_port_t notifyPort = MACH_PORT_NULL;

static Surface *find(uint32_t surfaceID) {
	for (Surface *s = surfaces; s; s = s->next)
		if (s->surfaceID == surfaceID)
			return s;
	return NULL;
}

/* ---- Surfaces in GPU memory ---- */

/* One connection per GPU, by registry entry ID, kept. */
typedef struct GPU {
	uint64_t	entryID;
	io_connect_t	connect;
	struct GPU	*next;
} GPU;
static GPU *gpus;

static io_connect_t gpuConnect(uint64_t entryID) {
	for (GPU *g = gpus; g; g = g->next)
		if (g->entryID == entryID)
			return g->connect;
	io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IORegistryEntryIDMatching(entryID));
	io_connect_t connect = IO_OBJECT_NULL;
	if (service == IO_OBJECT_NULL || !IOObjectConformsTo(service, "MinkarAMDGPU") ||
	    IOServiceOpen(service, mach_task_self(), 0, &connect) != KERN_SUCCESS)
		connect = IO_OBJECT_NULL;
	if (service != IO_OBJECT_NULL)
		IOObjectRelease(service);
	if (connect == IO_OBJECT_NULL)
		return IO_OBJECT_NULL;
	GPU *g = calloc(1, sizeof(*g));
	g->entryID = entryID;
	g->connect = connect;
	g->next = gpus;
	gpus = g;
	return connect;
}

static void destroy(Surface *s) {
	Surface **p = &surfaces;

	while (*p != s)
		p = &(*p)->next;
	*p = s->next;
	/* Its GPU memory goes when the GPU's last holder lets it go. */
	if (s->gpuBuffer != 0) {
		uint64_t b = s->gpuBuffer;
		IOConnectCallScalarMethod(s->gpu, kMinkarAMDGPUDestroyBuffer, &b, 1, NULL, NULL);
	}
	mach_port_deallocate(mach_task_self(), s->memory);
	if (s->port != MACH_PORT_NULL)
		mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_RECEIVE, -1);
	while (s->holds) {
		Hold *h = s->holds;
		s->holds = h->next;
		free(h);
	}
	while (s->grants) {
		Hold *h = s->grants;
		s->grants = h->next;
		free(h);
	}
	free(s->properties);
	free(s->values);
	free(s);
}

static void hold(Surface *s, pid_t pid) {
	for (Hold *h = s->holds; h; h = h->next)
		if (h->pid == pid) {
			h->count++;
			return;
		}
	Hold *h = calloc(1, sizeof(*h));
	h->pid = pid;
	h->count = 1;
	h->next = s->holds;
	s->holds = h;
}

/* Drops one hold of pid (all of them with all); true when none is left. */
static int unhold(Surface *s, pid_t pid, int all) {
	for (Hold **p = &s->holds; *p; p = &(*p)->next)
		if ((*p)->pid == pid) {
			if (all || --(*p)->count == 0) {
				Hold *h = *p;
				*p = h->next;
				free(h);
			}
			break;
		}
	return s->holds == NULL;
}

static int holds(Surface *s, pid_t pid) {
	for (Hold *h = s->holds; h; h = h->next)
		if (h->pid == pid)
			return 1;
	return 0;
}

/* Takes pid's grant (port_id's); true if it had one. */
static int takeGrant(Surface *s, pid_t pid) {
	for (Hold **p = &s->grants; *p; p = &(*p)->next)
		if ((*p)->pid == pid) {
			Hold *g = *p;
			*p = g->next;
			free(g);
			return 1;
		}
	return 0;
}

static void grant(Surface *s, pid_t pid) {
	for (Hold *g = s->grants; g; g = g->next)
		if (g->pid == pid)
			return;
	Hold *g = calloc(1, sizeof(*g));
	g->pid = pid;
	g->next = s->grants;
	s->grants = g;
}

/*
 * Root, or a program signed with the gpu-client entitlement: the window
 * server. MinkarCodeIntegrity loads no program with that (restricted)
 * entitlement unless the static trust cache has it, so the kernel's copy
 * of its entitlements is to be believed.
 */
#define kGPUClientEntitlement	"org.minkar.gpu-client"

static int privileged(audit_token_t token) {
	if (audit_token_to_euid(token) == 0)
		return 1;
	struct { uint32_t magic, length; } header;
	pid_t pid = audit_token_to_pid(token);
	header.length = 0;
	/* The blob's size first (ERANGE fills in the header), then all of it. */
	csops_audittoken(pid, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header), &token);
	uint32_t length = ntohl(header.length);
	if (length <= sizeof(header) || length > 65536)
		return 0;
	char *blob = calloc(1, length + 1);
	int yes = 0;
	if (blob != NULL &&
	    csops_audittoken(pid, CS_OPS_ENTITLEMENTS_BLOB, blob, length, &token) == 0) {
		CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)blob + sizeof(header),
		    length - sizeof(header), kCFAllocatorNull);
		CFPropertyListRef plist = data ? CFPropertyListCreateWithData(NULL, data,
		    kCFPropertyListImmutable, NULL, NULL) : NULL;
		if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID())
			yes = CFDictionaryGetValue(plist, CFSTR(kGPUClientEntitlement)) == kCFBooleanTrue;
		if (plist != NULL)
			CFRelease(plist);
		if (data != NULL)
			CFRelease(data);
	}
	free(blob);
	return yes;
}

/* What a holder (or a privileged process) may do with a surface. */
static int mayUse(Surface *s, audit_token_t token) {
	return s != NULL && (holds(s, audit_token_to_pid(token)) || privileged(token));
}

/* Whether the properties (a binary property list) make it global. */
static Boolean isGlobal(const void *bytes, size_t len) {
	Boolean global = false;
	CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, bytes, (CFIndex)len, kCFAllocatorNull);
	CFPropertyListRef p = data ? CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable,
	    NULL, NULL) : NULL;
	if (p != NULL && CFGetTypeID(p) == CFDictionaryGetTypeID())
		global = CFDictionaryGetValue(p, CFSTR("IOSurfaceIsGlobal")) == kCFBooleanTrue;
	if (p != NULL)
		CFRelease(p);
	if (data != NULL)
		CFRelease(data);
	return global;
}

/* The caller's session: its port is watched for the process's death. */
static void noteSession(pid_t pid, mach_port_t port) {
	mach_port_t previous = MACH_PORT_NULL;

	for (Session *s = sessions; s; s = s->next)
		if (s->pid == pid) {
			mach_port_deallocate(mach_task_self(), port);
			return;
		}
	if (mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 1,
	    notifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) != KERN_SUCCESS) {
		mach_port_deallocate(mach_task_self(), port);
		return;
	}
	if (previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
	Session *s = calloc(1, sizeof(*s));
	s->pid = pid;
	s->port = port;
	s->next = sessions;
	sessions = s;
}

static void sessionDied(mach_port_t name) {
	for (Session **p = &sessions; *p; p = &(*p)->next)
		if ((*p)->port == name) {
			Session *gone = *p;
			*p = gone->next;
			for (Surface *s = surfaces, *next; s; s = next) {
				next = s->next;
				while (takeGrant(s, gone->pid))
					;
				if (unhold(s, gone->pid, 1))
					destroy(s);
			}
			mach_port_deallocate(mach_task_self(), gone->port);
			free(gone);
			break;
		}
	/* The notification's own reference. */
	mach_port_deallocate(mach_task_self(), name);
}

static void *copyBytes(const void *bytes, size_t len) {
	void *copy = len ? malloc(len) : NULL;

	if (copy)
		memcpy(copy, bytes, len);
	return copy;
}

/* Out-of-line bytes for a reply (MIG deallocates them after sending). */
static void replyBytes(const void *bytes, size_t len, uint8_t **out, mach_msg_type_number_t *outLen) {
	vm_address_t addr = 0;

	*out = NULL;
	*outLen = 0;
	if (len == 0 || vm_allocate(mach_task_self(), &addr, len, VM_FLAGS_ANYWHERE) != KERN_SUCCESS)
		return;
	memcpy((void *)addr, bytes, len);
	*out = (uint8_t *)addr;
	*outLen = (mach_msg_type_number_t)len;
}

kern_return_t
iosd_create(mach_port_t server, audit_token_t token, mach_port_t session, mach_port_t memory,
    uint64_t allocSize, uint8_t *properties, mach_msg_type_number_t propertiesCnt,
    uint32_t *surfaceID, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	Surface *s = calloc(1, sizeof(*s));

	noteSession(pid, session);
	s->surfaceID = nextID++;
	if (nextID == 0)
		nextID = 1;
	s->memory = memory;
	s->allocSize = allocSize;
	s->properties = copyBytes(properties, propertiesCnt);
	s->propertiesLen = propertiesCnt;
	s->global = isGlobal(properties, propertiesCnt);
	s->next = surfaces;
	surfaces = s;
	hold(s, pid);
	mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)properties, propertiesCnt);
	*surfaceID = s->surfaceID;
	*result = 0;
	return KERN_SUCCESS;
}

kern_return_t
iosd_lookup(mach_port_t server, audit_token_t token, mach_port_t session, uint32_t surfaceID,
    mach_port_t *memory, uint64_t *allocSize, uint8_t **properties,
    mach_msg_type_number_t *propertiesCnt, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	Surface *s = find(surfaceID);

	noteSession(pid, session);
	*memory = MACH_PORT_NULL;
	*allocSize = 0;
	*properties = NULL;
	*propertiesCnt = 0;
	/* The ID alone is not enough (see the top of this file). */
	if (s == NULL || !(s->global || holds(s, pid) || takeGrant(s, pid) || privileged(token))) {
		*result = -1;
		return KERN_SUCCESS;
	}
	/* A send right of its own for the caller: the reply moves it. */
	if (mach_port_mod_refs(mach_task_self(), s->memory, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) {
		*result = -1;
		return KERN_SUCCESS;
	}
	hold(s, pid);
	*memory = s->memory;
	*allocSize = s->allocSize;
	replyBytes(s->properties, s->propertiesLen, properties, propertiesCnt);
	*result = 0;
	return KERN_SUCCESS;
}

kern_return_t
iosd_release(mach_port_t server, audit_token_t token, uint32_t surfaceID)
{
	Surface *s = find(surfaceID);

	if (s != NULL && unhold(s, audit_token_to_pid(token), 0))
		destroy(s);
	return KERN_SUCCESS;
}

kern_return_t
iosd_attach_gpu_buffer(mach_port_t server, audit_token_t token, uint32_t surfaceID, uint64_t gpu,
    uint64_t buffer, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	Surface *s = find(surfaceID);
	int holds = 0;

	*result = -1;
	if (s != NULL)
		for (Hold *h = s->holds; h; h = h->next)
			if (h->pid == pid)
				holds = 1;
	if (s == NULL || !holds || s->gpuBuffer != 0 || buffer == 0)
		return KERN_SUCCESS;
	io_connect_t c = gpuConnect(gpu);
	uint64_t in = buffer, out[3];
	uint32_t n = 3;
	if (c == IO_OBJECT_NULL ||
	    IOConnectCallScalarMethod(c, kMinkarAMDGPURetainSharedBuffer, &in, 1, out, &n) != KERN_SUCCESS)
		return KERN_SUCCESS;
	/* It must hold the whole surface: who draws it reads that much. */
	if (out[2] < s->allocSize) {
		uint64_t b = out[0];
		IOConnectCallScalarMethod(c, kMinkarAMDGPUDestroyBuffer, &b, 1, NULL, NULL);
		return KERN_SUCCESS;
	}
	s->gpu = c;
	s->gpuBuffer = out[0];
	s->gpuEntryID = gpu;
	s->gpuName = buffer;
	*result = 0;
	return KERN_SUCCESS;
}

kern_return_t
iosd_gpu_buffer(mach_port_t server, audit_token_t token, uint32_t surfaceID, uint64_t *gpu,
    uint64_t *buffer, int32_t *result)
{
	Surface *s = find(surfaceID);

	if (!mayUse(s, token))
		s = NULL;

	*gpu = s != NULL ? s->gpuEntryID : 0;
	*buffer = s != NULL ? s->gpuName : 0;
	*result = s != NULL ? 0 : -1;
	return KERN_SUCCESS;
}

kern_return_t
iosd_set_values(mach_port_t server, audit_token_t token, uint32_t surfaceID, uint8_t *values,
    mach_msg_type_number_t valuesCnt, int32_t *result)
{
	Surface *s = find(surfaceID);

	if (!mayUse(s, token))
		s = NULL;

	if (s != NULL) {
		free(s->values);
		s->values = copyBytes(values, valuesCnt);
		s->valuesLen = valuesCnt;
	}
	mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)values, valuesCnt);
	*result = s ? 0 : -1;
	return KERN_SUCCESS;
}

kern_return_t
iosd_copy_values(mach_port_t server, audit_token_t token, uint32_t surfaceID, uint8_t **values,
    mach_msg_type_number_t *valuesCnt, int32_t *result)
{
	Surface *s = find(surfaceID);

	if (!mayUse(s, token))
		s = NULL;

	*values = NULL;
	*valuesCnt = 0;
	if (s == NULL) {
		*result = -1;
		return KERN_SUCCESS;
	}
	replyBytes(s->values, s->valuesLen, values, valuesCnt);
	*result = 0;
	return KERN_SUCCESS;
}

kern_return_t
iosd_make_port(mach_port_t server, audit_token_t token, uint32_t surfaceID, mach_port_t *port,
    int32_t *result)
{
	Surface *s = find(surfaceID);

	if (!mayUse(s, token))
		s = NULL;

	*port = MACH_PORT_NULL;
	if (s == NULL) {
		*result = -1;
		return KERN_SUCCESS;
	}
	if (s->port == MACH_PORT_NULL &&
	    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->port) != KERN_SUCCESS) {
		s->port = MACH_PORT_NULL;
		*result = -1;
		return KERN_SUCCESS;
	}
	*port = s->port;
	*result = 0;
	return KERN_SUCCESS;
}

/* A port sent back to us arrives as the name of our receive right. */
kern_return_t
iosd_port_id(mach_port_t server, audit_token_t token, mach_port_t port, uint32_t *surfaceID,
    int32_t *result)
{
	*surfaceID = 0;
	*result = -1;
	for (Surface *s = surfaces; s; s = s->next)
		if (s->port != MACH_PORT_NULL && s->port == port) {
			/* Who has the port may look the surface up, once. */
			grant(s, audit_token_to_pid(token));
			*surfaceID = s->surfaceID;
			*result = 0;
			return KERN_SUCCESS;
		}
	/* Another port: drop the right the message brought. */
	if (port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), port);
	return KERN_SUCCESS;
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, kService, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "iosurfaced: cannot check in %s: 0x%x\n", kService, kr);
		return 1;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) !=
	    KERN_SUCCESS) {
		fprintf(stderr, "iosurfaced: no notification port\n");
		return 1;
	}

	/* One queue: requests and deaths are served in order. */
	dispatch_queue_t queue = dispatch_queue_create("org.minkar.iosurfaced", DISPATCH_QUEUE_SERIAL);
	dispatch_source_t notifySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    notifyPort, 0, queue);
	dispatch_source_set_event_handler(notifySource, ^{
		union {
			mach_dead_name_notification_t dead;
			uint8_t bytes[sizeof(mach_dead_name_notification_t) + MAX_TRAILER_SIZE];
		} msg;

		while (mach_msg(&msg.dead.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
		    sizeof(msg), notifyPort, 0, MACH_PORT_NULL) == MACH_MSG_SUCCESS)
			if (msg.dead.not_header.msgh_id == MACH_NOTIFY_DEAD_NAME)
				sessionDied(msg.dead.not_port);
	});
	dispatch_resume(notifySource);

	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, service, 0,
	    queue);
	dispatch_source_set_event_handler(source, ^{
		/* Room for the audit trailer too. */
		mach_msg_server_once(IOSurfaceRegistry_server,
		    iosd_IOSurfaceRegistry_subsystem.maxsize + MAX_TRAILER_SIZE, service,
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(source);
	fprintf(stderr, "iosurfaced: serving %s\n", kService);
	dispatch_main();
}
