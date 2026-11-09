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
 * pboard: the pasteboard server (Phase 4). Named pasteboards, each a
 * change count and data by type, kept in memory by the caller's user ID:
 * a user's pasteboards are the user's own. Started on demand by launchd
 * (org.minkar.pboard); its interface is Frameworks/AppKit/Pasteboard.
 */

#include <bsm/libbsm.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include "PasteboardServer.h"	/* MIG, generated */

#define kService	"org.minkar.pboard"

/* What one user's pasteboards may hold, all together. */
#define kMaxBytesPerUser	(256ULL << 20)

typedef struct Pasteboard {
	char			name[256];
	int32_t			count;
	CFMutableArrayRef	types;		/* CFString, in the order written */
	CFMutableDictionaryRef	data;		/* type -> CFData */
	uint64_t		bytes;
	struct Pasteboard	*next;
} Pasteboard;

typedef struct User {
	uid_t			uid;
	uint64_t		bytes;
	Pasteboard		*pasteboards;
	struct User		*next;
} User;

static User *users;

static User *
userFor(audit_token_t token)
{
	uid_t uid = audit_token_to_euid(token);

	for (User *u = users; u; u = u->next)
		if (u->uid == uid)
			return u;
	User *u = calloc(1, sizeof(*u));
	u->uid = uid;
	u->next = users;
	users = u;
	return u;
}

static Pasteboard *
find(User *u, const char *name, int create)
{
	for (Pasteboard *p = u->pasteboards; p; p = p->next)
		if (strcmp(p->name, name) == 0)
			return p;
	if (!create || name[0] == '\0')
		return NULL;
	Pasteboard *p = calloc(1, sizeof(*p));
	strlcpy(p->name, name, sizeof(p->name));
	p->types = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	p->data = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	p->next = u->pasteboards;
	u->pasteboards = p;
	return p;
}

static void
empty(User *u, Pasteboard *p)
{
	CFArrayRemoveAllValues(p->types);
	CFDictionaryRemoveAllValues(p->data);
	u->bytes -= p->bytes;
	p->bytes = 0;
}

static CFStringRef
typeName(const char *type)
{
	return CFStringCreateWithCString(NULL, type, kCFStringEncodingUTF8);
}

kern_return_t
pbs_change_count(mach_port_t server, audit_token_t token, pb_name_t name, int32_t *count)
{
	Pasteboard *p = find(userFor(token), name, 0);

	*count = p != NULL ? p->count : 0;
	return KERN_SUCCESS;
}

kern_return_t
pbs_clear(mach_port_t server, audit_token_t token, pb_name_t name, int32_t *count)
{
	User *u = userFor(token);
	Pasteboard *p = find(u, name, 1);

	*count = 0;
	if (p == NULL)
		return KERN_SUCCESS;
	empty(u, p);
	p->count++;
	if (p->count <= 0)	/* wrapped: change counts stay positive */
		p->count = 1;
	*count = p->count;
	return KERN_SUCCESS;
}

kern_return_t
pbs_set_data(mach_port_t server, audit_token_t token, pb_name_t name, int32_t count,
    pb_name_t type, uint8_t *bytes, mach_msg_type_number_t bytesCnt, int32_t *result)
{
	User *u = userFor(token);
	Pasteboard *p = find(u, name, 0);
	CFStringRef t = NULL;

	*result = -1;
	if (p == NULL || p->count != count || type[0] == '\0')
		goto done;
	t = typeName(type);
	if (t == NULL)
		goto done;
	CFDataRef old = CFDictionaryGetValue(p->data, t);
	uint64_t oldBytes = old != NULL ? (uint64_t)CFDataGetLength(old) : 0;
	if (u->bytes - oldBytes + bytesCnt > kMaxBytesPerUser)
		goto done;
	CFDataRef d = CFDataCreate(NULL, bytes, bytesCnt);
	if (d == NULL)
		goto done;
	if (old == NULL)
		CFArrayAppendValue(p->types, t);
	CFDictionarySetValue(p->data, t, d);
	CFRelease(d);
	u->bytes = u->bytes - oldBytes + bytesCnt;
	p->bytes = p->bytes - oldBytes + bytesCnt;
	*result = 0;
done:
	if (t != NULL)
		CFRelease(t);
	mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)(uintptr_t)bytes, bytesCnt);
	return KERN_SUCCESS;
}

/* Out-of-line bytes for a reply (MIG deallocates them after sending). */
static void
replyBytes(const void *bytes, size_t len, uint8_t **out, mach_msg_type_number_t *outLen)
{
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
pbs_types(mach_port_t server, audit_token_t token, pb_name_t name, uint8_t **types,
    mach_msg_type_number_t *typesCnt, int32_t *count)
{
	Pasteboard *p = find(userFor(token), name, 0);
	char *list = NULL;
	size_t len = 0;

	*count = p != NULL ? p->count : 0;
	*types = NULL;
	*typesCnt = 0;
	if (p == NULL)
		return KERN_SUCCESS;
	for (CFIndex i = 0; i < CFArrayGetCount(p->types); i++) {
		char t[256];
		if (!CFStringGetCString(CFArrayGetValueAtIndex(p->types, i), t, sizeof(t),
		    kCFStringEncodingUTF8))
			continue;
		size_t n = strlen(t) + 1;
		char *grown = realloc(list, len + n);
		if (grown == NULL)
			break;
		list = grown;
		memcpy(list + len, t, n);
		len += n;
	}
	replyBytes(list, len, types, typesCnt);
	free(list);
	return KERN_SUCCESS;
}

kern_return_t
pbs_get_data(mach_port_t server, audit_token_t token, pb_name_t name, pb_name_t type,
    uint8_t **data, mach_msg_type_number_t *dataCnt, int32_t *result)
{
	Pasteboard *p = find(userFor(token), name, 0);
	CFStringRef t = typeName(type);
	CFDataRef d = p != NULL && t != NULL ? CFDictionaryGetValue(p->data, t) : NULL;

	*data = NULL;
	*dataCnt = 0;
	*result = d != NULL ? 0 : -1;
	if (d != NULL)
		replyBytes(CFDataGetBytePtr(d), (size_t)CFDataGetLength(d), data, dataCnt);
	if (t != NULL)
		CFRelease(t);
	return KERN_SUCCESS;
}

kern_return_t
pbs_release(mach_port_t server, audit_token_t token, pb_name_t name)
{
	User *u = userFor(token);

	for (Pasteboard **pp = &u->pasteboards; *pp; pp = &(*pp)->next)
		if (strcmp((*pp)->name, name) == 0) {
			Pasteboard *p = *pp;
			*pp = p->next;
			empty(u, p);
			CFRelease(p->types);
			CFRelease(p->data);
			free(p);
			break;
		}
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
		fprintf(stderr, "pboard: cannot check in %s: 0x%x\n", kService, kr);
		return 1;
	}
	/* One queue: requests are served in order. */
	dispatch_queue_t queue = dispatch_queue_create("org.minkar.pboard", DISPATCH_QUEUE_SERIAL);
	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, service, 0,
	    queue);
	dispatch_source_set_event_handler(source, ^{
		/* Room for the largest request (the subsystem's maxsize is
		   its largest reply: a request with long names did not fit,
		   and was dropped), and for the audit trailer. */
		mach_msg_server_once(Pasteboard_server,
		    (mach_msg_size_t)MAX(sizeof(union __RequestUnion__pbs_Pasteboard_subsystem),
		    pbs_Pasteboard_subsystem.maxsize) + MAX_TRAILER_SIZE, service,
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(source);
	fprintf(stderr, "pboard: serving %s\n", kService);
	dispatch_main();
}
