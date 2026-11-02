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
 * Started on demand by launchd (org.minkar.iosurfaced).
 */

#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "IOSurfaceRegistryServer.h"	/* MIG, generated */

#define kService	"org.minkar.iosurfaced"

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

static void destroy(Surface *s) {
	Surface **p = &surfaces;

	while (*p != s)
		p = &(*p)->next;
	*p = s->next;
	mach_port_deallocate(mach_task_self(), s->memory);
	if (s->port != MACH_PORT_NULL)
		mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_RECEIVE, -1);
	while (s->holds) {
		Hold *h = s->holds;
		s->holds = h->next;
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
	if (s == NULL) {
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
iosd_set_values(mach_port_t server, uint32_t surfaceID, uint8_t *values,
    mach_msg_type_number_t valuesCnt, int32_t *result)
{
	Surface *s = find(surfaceID);

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
iosd_copy_values(mach_port_t server, uint32_t surfaceID, uint8_t **values,
    mach_msg_type_number_t *valuesCnt, int32_t *result)
{
	Surface *s = find(surfaceID);

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
iosd_make_port(mach_port_t server, uint32_t surfaceID, mach_port_t *port, int32_t *result)
{
	Surface *s = find(surfaceID);

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
iosd_port_id(mach_port_t server, mach_port_t port, uint32_t *surfaceID, int32_t *result)
{
	*surfaceID = 0;
	*result = -1;
	for (Surface *s = surfaces; s; s = s->next)
		if (s->port != MACH_PORT_NULL && s->port == port) {
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
