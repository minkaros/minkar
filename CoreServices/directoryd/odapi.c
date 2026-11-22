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
 * OpenDirectory's service (the framework's CFOpenDirectory talks to it,
 * odxpc.c): sessions, nodes, queries and records, answered from the local
 * account files, the node /Local/Default (/Search gives the same). Users
 * and groups are records with the reference's attributes (RecordName,
 * UniqueID, PrimaryGroupID, RealName, NFSHomeDirectory, UserShell,
 * GeneratedUID, Password "********"; a group's GroupMembership), each
 * with AppleMetaNodeLocation. A password is verified against the
 * account's crypt(3) hash; a user may change their own password with the
 * old one, root any. Root, or a node authenticated as an administrator
 * (ODNodeSetCredentials, a member of admin), may also set a user's
 * RealName and UserShell, create a user (sysadminctl -addUser: its home
 * from the user template, staff, zsh) and delete one (sysadminctl
 * -deleteUser, keeping its home), add a user to a group and take it out
 * (GroupMembership, as the Users pane's "Allow user to administer this
 * computer" does with admin), and, through ODNodeCustomCall, Minkar's
 * own: set the login window's options (kCustomLoginOptions), remove the
 * home of a user already deleted (kCustomRemoveHome) and set the system's
 * time zone (kCustomTimeZone, for the Date & Time pane, as systemsetup
 * -settimezone does).
 *
 * The protocol (opendirectory/odconstants.h, odutils.h): each request
 * carries its client's ID and a property list { function, values }; each
 * reply the same ID, a result of the same shape, an error code and
 * whether it is the request's last. A query answers at once, all its
 * records, and ends; to synchronize, the framework makes it again.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <xpc/xpc.h>
#include <errno.h>

#include <opendirectory/odconstants.h>
#include <opendirectory/odutils.h>

#include "pw_compat.h"

#include "odapi.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <notify.h>
#include <limits.h>

extern char **environ;

#define GROUP_FILE	"/etc/group"
#define LOGIN_PREFS	"/Library/Preferences/com.apple.loginwindow.plist"
#define SYSADMINCTL	"/usr/sbin/sysadminctl"

/* ODNodeCustomCall's codes, Minkar's own (the Users pane). */
#define kCustomLoginOptions	0x4c4f	/* 'LO': a dictionary of login window
					   options; an empty string removes one */
#define kCustomRemoveHome	0x524d	/* 'RM': a deleted user's name */
#define kCustomTimeZone		0x545a	/* 'TZ': a zone's name (Date & Time) */
#define ZONEINFO	"/var/db/timezone/zoneinfo"
#define LOCALTIME	"/etc/localtime"

#define LOCAL_NODE	CFSTR("/Local/Default")
#define SEARCH_NODE	CFSTR("/Search")
#define CONTACTS_NODE	CFSTR("/Contacts")

#define REC_USERS	CFSTR("dsRecTypeStandard:Users")
#define REC_GROUPS	CFSTR("dsRecTypeStandard:Groups")
#define REC_PREFIX	CFSTR("dsRecTypeStandard:")

#define A(name)		CFSTR("dsAttrTypeStandard:" name)
#define ALL_ATTRS	CFSTR("dsAttributesAll")
#define STD_ATTRS	CFSTR("dsAttributesStandardAll")
#define NATIVE_ATTRS	CFSTR("dsAttributesNativeAll")

/* The kinds of objects a UUID names. */
enum { OBJ_SESSION, OBJ_NODE };

struct object {
	uuid_t		uuid;
	int		kind;
	xpc_connection_t peer;
	CFStringRef	name;		/* a node's name */
	char		*authUser;	/* a node's authenticated user */
	struct object	*next;
};

static struct object *objects;
static dispatch_queue_t queue;

static struct object *
object_find(const uint8_t *uuid, int kind)
{
	if (uuid == NULL || uuid_is_null(uuid))
		return (NULL);
	for (struct object *o = objects; o != NULL; o = o->next)
		if (o->kind == kind && uuid_compare(o->uuid, uuid) == 0)
			return (o);
	return (NULL);
}

static struct object *
object_new(int kind, xpc_connection_t peer)
{
	struct object *o = calloc(1, sizeof(*o));

	uuid_generate_random(o->uuid);
	o->kind = kind;
	o->peer = peer;
	o->next = objects;
	objects = o;
	return (o);
}

static void
object_free(struct object *gone)
{
	for (struct object **p = &objects; *p != NULL; p = &(*p)->next)
		if (*p == gone) {
			*p = gone->next;
			if (gone->name != NULL)
				CFRelease(gone->name);
			free(gone->authUser);
			free(gone);
			return;
		}
}

/* Everything a connection opened, when it goes. */
static void
objects_drop(xpc_connection_t peer)
{
	struct object *o = objects, *next;

	for (; o != NULL; o = next) {
		next = o->next;
		if (o->peer == peer)
			object_free(o);
	}
}

static CFDataRef
uuid_data(const uuid_t uuid)
{
	return (CFDataCreate(NULL, uuid, sizeof(uuid_t)));
}

/* ---- Records ----------------------------------------------------------- */

static void
put(CFMutableDictionaryRef rec, CFStringRef attr, const char *value)
{
	CFStringRef s;
	CFArrayRef a;

	if (value == NULL)
		return;
	s = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
	if (s == NULL)
		return;
	a = CFArrayCreate(NULL, (const void **)&s, 1, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(rec, attr, a);
	CFRelease(a);
	CFRelease(s);
}

static void
put_number(CFMutableDictionaryRef rec, CFStringRef attr, long value)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%ld", value);
	put(rec, attr, buf);
}

/* The GeneratedUID of an account with no UUID of its own: fixed by its
   kind and ID, as the reference gives its built-in accounts. */
static void
put_generated_uid(CFMutableDictionaryRef rec, bool user, long id)
{
	char buf[40];

	snprintf(buf, sizeof(buf), "%s-%08lX", user ? "FFFFEEEE-DDDD-CCCC-BBBB-AAAA" :
	    "ABCDEFAB-CDEF-ABCD-EFAB-CDEF", (unsigned long)id & 0xffffffffUL);
	put(rec, A("GeneratedUID"), buf);
}

static CFMutableDictionaryRef
record_new(CFStringRef type)
{
	CFMutableDictionaryRef rec = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef a;

	a = CFArrayCreate(NULL, (const void **)&type, 1, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(rec, A("RecordType"), a);
	CFRelease(a);
	a = CFArrayCreate(NULL, (const void *[]){ LOCAL_NODE }, 1, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(rec, A("AppleMetaNodeLocation"), a);
	CFRelease(a);
	return (rec);
}

static CFMutableDictionaryRef
user_record(const struct passwd *pw)
{
	CFMutableDictionaryRef rec = record_new(REC_USERS);

	put(rec, A("RecordName"), pw->pw_name);
	put_number(rec, A("UniqueID"), (long)pw->pw_uid);
	put_number(rec, A("PrimaryGroupID"), (long)pw->pw_gid);
	put(rec, A("RealName"), pw->pw_gecos != NULL && *pw->pw_gecos != '\0' ?
	    pw->pw_gecos : pw->pw_name);
	put(rec, A("NFSHomeDirectory"), pw->pw_dir);
	put(rec, A("UserShell"), pw->pw_shell);
	put(rec, A("Password"), "********");
	put_generated_uid(rec, true, (long)pw->pw_uid);
	return (rec);
}

static CFMutableDictionaryRef
group_record(const struct group *gr)
{
	CFMutableDictionaryRef rec = record_new(REC_GROUPS);
	CFMutableArrayRef members = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	put(rec, A("RecordName"), gr->gr_name);
	put_number(rec, A("PrimaryGroupID"), (long)gr->gr_gid);
	put(rec, A("RealName"), gr->gr_name);
	put(rec, A("Password"), "*");
	put_generated_uid(rec, false, (long)gr->gr_gid);
	for (char **m = gr->gr_mem; m != NULL && *m != NULL; m++) {
		CFStringRef s = CFStringCreateWithCString(NULL, *m, kCFStringEncodingUTF8);
		if (s != NULL) {
			CFArrayAppendValue(members, s);
			CFRelease(s);
		}
	}
	if (CFArrayGetCount(members) > 0)
		CFDictionarySetValue(rec, A("GroupMembership"), members);
	CFRelease(members);
	return (rec);
}

/* The record types the reference lists on its local node: known, and
   empty here unless they are users or groups. */
static bool
record_type_known(CFStringRef type)
{
	static const char *const known[] = {
		"AFPUserAliases", "Aliases", "Automount", "AutomountMap",
		"ComputerGroups", "ComputerLists", "Computers", "Config",
		"Ethernets", "Groups", "Hosts", "Mounts", "NetGroups",
		"Networks", "People", "PresetComputerGroups",
		"PresetComputerLists", "PresetComputers", "PresetGroups",
		"PresetUsers", "Protocols", "RPC", "Services", "SharePoints",
		"Users", NULL
	};
	char buf[128];

	if (!CFStringHasPrefix(type, REC_PREFIX) ||
	    !CFStringGetCString(type, buf, sizeof(buf), kCFStringEncodingUTF8))
		return (false);
	for (int i = 0; known[i] != NULL; i++)
		if (strcmp(buf + strlen("dsRecTypeStandard:"), known[i]) == 0)
			return (true);
	return (false);
}

/* ---- Matching ---------------------------------------------------------- */

static bool
value_matches(CFStringRef have, CFStringRef want, uint32_t match)
{
	CFStringCompareFlags f = (match & 0x0100) ? kCFCompareCaseInsensitive : 0;
	CFRange whole = CFRangeMake(0, CFStringGetLength(have));
	CFComparisonResult c;

	switch (match & ~0x0100u) {
	case 0x2001:	/* equal to */
		return (CFStringCompare(have, want, f) == kCFCompareEqualTo);
	case 0x2002:	/* begins with */
		return (CFStringFindWithOptions(have, want, whole, f | kCFCompareAnchored, NULL));
	case 0x2003:	/* ends with */
		return (CFStringFindWithOptions(have, want, whole,
		    f | kCFCompareAnchored | kCFCompareBackwards, NULL));
	case 0x2004:	/* contains */
		return (CFStringFindWithOptions(have, want, whole, f, NULL));
	case 0x2005:	/* greater than */
	case 0x2006:	/* less than */
		c = CFStringCompare(have, want, f | kCFCompareNumerically);
		return ((match & ~0x0100u) == 0x2005 ? c == kCFCompareGreaterThan :
		    c == kCFCompareLessThan);
	}
	return (false);
}

static bool
record_matches(CFDictionaryRef rec, CFStringRef attr, uint32_t match, CFArrayRef wanted)
{
	CFArrayRef values;

	if (match == 0x0001)	/* any */
		return (true);
	if (wanted == NULL)
		return (false);
	values = attr != NULL ? CFDictionaryGetValue(rec, attr) : NULL;
	if (values == NULL || CFGetTypeID(values) != CFArrayGetTypeID())
		return (false);
	for (CFIndex i = 0; i < CFArrayGetCount(values); i++) {
		CFStringRef have = CFArrayGetValueAtIndex(values, i);
		if (CFGetTypeID(have) != CFStringGetTypeID())
			continue;
		for (CFIndex j = 0; j < CFArrayGetCount(wanted); j++) {
			CFStringRef want = CFArrayGetValueAtIndex(wanted, j);
			if (CFGetTypeID(want) == CFStringGetTypeID() &&
			    value_matches(have, want, match))
				return (true);
		}
	}
	return (false);
}

/* The attributes asked for: all of them, or only those listed, with the
   record's name, type and node always. */
static CFDictionaryRef
record_trim(CFDictionaryRef rec, CFArrayRef attrs)
{
	CFMutableDictionaryRef out;
	CFStringRef keep[] = { A("RecordName"), A("RecordType"), A("AppleMetaNodeLocation") };

	if (attrs == NULL || CFArrayContainsValue(attrs, CFRangeMake(0, CFArrayGetCount(attrs)), ALL_ATTRS) ||
	    CFArrayContainsValue(attrs, CFRangeMake(0, CFArrayGetCount(attrs)), STD_ATTRS))
		return (CFRetain(rec));
	out = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	for (CFIndex i = 0; i < CFArrayGetCount(attrs); i++) {
		CFTypeRef v = CFDictionaryGetValue(rec, CFArrayGetValueAtIndex(attrs, i));
		if (v != NULL)
			CFDictionarySetValue(out, CFArrayGetValueAtIndex(attrs, i), v);
	}
	for (size_t i = 0; i < sizeof(keep) / sizeof(keep[0]); i++) {
		CFTypeRef v = CFDictionaryGetValue(rec, keep[i]);
		if (v != NULL)
			CFDictionarySetValue(out, keep[i], v);
	}
	return (out);
}

static CFArrayRef
as_array(CFTypeRef v)
{
	if (v == NULL)
		return (NULL);
	if (CFGetTypeID(v) == CFArrayGetTypeID())
		return (CFRetain(v));
	return (CFArrayCreate(NULL, &v, 1, &kCFTypeArrayCallBacks));
}

/* The records of types that match; *error a code when a type is unknown. */
static CFArrayRef
records_find(CFArrayRef types, CFStringRef attr, uint32_t match, CFArrayRef wanted,
    CFArrayRef attrs, CFIndex max, uint32_t *error)
{
	CFMutableArrayRef found = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	*error = 0;
	for (CFIndex t = 0; types != NULL && t < CFArrayGetCount(types); t++) {
		CFStringRef type = CFArrayGetValueAtIndex(types, t);
		bool users, groups;

		if (CFGetTypeID(type) != CFStringGetTypeID() || !record_type_known(type)) {
			*error = kODErrorRecordAttributeUnknownType;
			break;
		}
		users = CFEqual(type, REC_USERS);
		groups = CFEqual(type, REC_GROUPS);
		if (users) {
			struct passwd *pw;
			setpwent();
			while ((pw = getpwent()) != NULL && (max == 0 || CFArrayGetCount(found) < max)) {
				CFMutableDictionaryRef rec = user_record(pw);
				if (record_matches(rec, attr, match, wanted)) {
					CFDictionaryRef trimmed = record_trim(rec, attrs);
					CFArrayAppendValue(found, trimmed);
					CFRelease(trimmed);
				}
				CFRelease(rec);
			}
			endpwent();
		} else if (groups) {
			struct group *gr;
			setgrent();
			while ((gr = getgrent()) != NULL && (max == 0 || CFArrayGetCount(found) < max)) {
				CFMutableDictionaryRef rec = group_record(gr);
				if (record_matches(rec, attr, match, wanted)) {
					CFDictionaryRef trimmed = record_trim(rec, attrs);
					CFArrayAppendValue(found, trimmed);
					CFRelease(trimmed);
				}
				CFRelease(rec);
			}
			endgrent();
		}
	}
	return (found);
}

/* ---- Accounts ---------------------------------------------------------- */

static char *
cstring(CFTypeRef s)
{
	char *buf;
	CFIndex len;

	if (s == NULL || CFGetTypeID(s) != CFStringGetTypeID())
		return (NULL);
	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
	buf = malloc((size_t)len);
	if (buf != NULL && !CFStringGetCString(s, buf, len, kCFStringEncodingUTF8)) {
		free(buf);
		buf = NULL;
	}
	return (buf);
}

/* A user's password, checked against the account's hash: 0, or a code. */
static uint32_t
password_check(const char *user, const char *password)
{
	struct passwd *pw;
	const char *hash;
	char *got;

	if (user == NULL || password == NULL)
		return (kODErrorCredentialsParameterError);
	pw = getpwnam(user);
	if (pw == NULL)
		return (kODErrorCredentialsInvalid);
	hash = pw->pw_passwd;
	if (hash == NULL || hash[0] == '\0' || hash[0] == '*' || hash[0] == '!')
		return (strncmp(hash != NULL ? hash : "", "*LOCKED*", 8) == 0 ?
		    kODErrorCredentialsAccountDisabled : kODErrorCredentialsInvalid);
	got = crypt(password, hash);
	if (got == NULL || strcmp(got, hash) != 0)
		return (kODErrorCredentialsInvalid);
	return (0);
}

static bool
is_admin(const char *user)
{
	struct group *gr;
	struct passwd *pw;

	if (user == NULL)
		return (false);
	if ((pw = getpwnam(user)) != NULL && pw->pw_uid == 0)
		return (true);
	if ((gr = getgrnam("admin")) == NULL)
		return (false);
	for (char **m = gr->gr_mem; m != NULL && *m != NULL; m++)
		if (strcmp(*m, user) == 0)
			return (true);
	return (pw != NULL && pw->pw_gid == gr->gr_gid);
}

static char *
hash_password(const char *password)
{
	static const char chars[] =
	    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char salt[3 + 16 + 2], *hash;

	memcpy(salt, "$6$", 3);
	for (int i = 0; i < 16; i++)
		salt[3 + i] = chars[arc4random_uniform(sizeof(chars) - 1)];
	salt[3 + 16] = '$';
	salt[3 + 16 + 1] = '\0';
	hash = crypt(password, salt);
	if (hash == NULL || strncmp(hash, "$6$", 3) != 0)
		return (NULL);
	return (strdup(hash));
}

/* The account changed by edit (on a copy of its entry), written to the
   account files: 0, or a code. */
static uint32_t
account_update(const char *user, void (^edit)(struct passwd *))
{
	struct passwd *old, *new;
	int pfd, tfd;
	uint32_t code = 0;

	if ((old = getpwnam(user)) == NULL)
		return (kODErrorRecordNoLongerExists);
	if ((old = pw_dup(old)) == NULL || (new = pw_dup(old)) == NULL) {
		free(old);
		return (kODErrorDaemonError);
	}
	edit(new);
	if (pw_init(NULL, NULL) != 0) {
		code = kODErrorDaemonError;
	} else if ((pfd = pw_lock()) == -1) {
		code = kODErrorDaemonError;
		pw_fini();
	} else {
		if ((tfd = pw_tmp(-1)) == -1 || pw_copy(pfd, tfd, new, old) == -1 ||
		    pw_mkdb(new->pw_name) == -1)
			code = kODErrorDaemonError;
		pw_fini();
	}
	free(new);
	free(old);
	return (code);
}

/* A tool, waited for: its exit status, or -1. */
static int
run(char *const argv[])
{
	pid_t pid;
	int status;

	if (posix_spawn(&pid, argv[0], NULL, NULL, argv, environ) != 0)
		return (-1);
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
		return (-1);
	return (WEXITSTATUS(status));
}

/* name added to, or taken out of, group's members in /etc/group (the
   file locked, written aside and renamed over it). */
static uint32_t
group_edit(const char *group, const char *name, bool add)
{
	char tmp[] = GROUP_FILE ".XXXXXX", line[4096];
	FILE *in, *out;
	struct stat st;
	int fd, lfd;
	bool found = false;

	if (getgrnam(group) == NULL)
		return (kODErrorRecordNoLongerExists);
	if ((lfd = open(GROUP_FILE, O_RDONLY)) < 0 || flock(lfd, LOCK_EX) != 0 ||
	    (in = fopen(GROUP_FILE, "r")) == NULL) {
		if (lfd >= 0)
			close(lfd);
		return (kODErrorDaemonError);
	}
	fstat(fileno(in), &st);
	if ((fd = mkstemp(tmp)) < 0 || (out = fdopen(fd, "w")) == NULL) {
		fclose(in);
		close(lfd);
		return (kODErrorDaemonError);
	}
	while (fgets(line, sizeof(line), in) != NULL) {
		char copy[4096], *fields[4], *p = copy, *m, *last;
		char members[4096] = "";
		bool present = false;
		int n = 0;

		strlcpy(copy, line, sizeof(copy));
		copy[strcspn(copy, "\n")] = '\0';
		while (n < 4 && (fields[n] = strsep(&p, ":")) != NULL)
			n++;
		if (n != 4 || copy[0] == '#' || strcmp(fields[0], group) != 0) {
			fputs(line, out);
			continue;
		}
		found = true;
		for (m = strtok_r(fields[3], ",", &last); m != NULL; m = strtok_r(NULL, ",", &last)) {
			if (strcmp(m, name) == 0) {
				present = true;
				if (!add)
					continue;
			}
			if (members[0] != '\0')
				strlcat(members, ",", sizeof(members));
			strlcat(members, m, sizeof(members));
		}
		if (add && !present) {
			if (members[0] != '\0')
				strlcat(members, ",", sizeof(members));
			strlcat(members, name, sizeof(members));
		}
		fprintf(out, "%s:%s:%s:%s\n", fields[0], fields[1], fields[2], members);
	}
	fclose(in);
	fchmod(fd, st.st_mode & 07777);
	fchown(fd, st.st_uid, st.st_gid);
	if (fclose(out) != 0 || rename(tmp, GROUP_FILE) != 0) {
		unlink(tmp);
		close(lfd);
		return (kODErrorDaemonError);
	}
	close(lfd);
	return (found ? 0 : kODErrorRecordNoLongerExists);
}

/* The login window's options the Users pane sets: values replaced, an
   empty string removes one; others refused. */
static uint32_t
login_options(CFDataRef data)
{
	static const CFStringRef allowed[] = { CFSTR("autoLoginUser"), CFSTR("SHOWFULLNAME"),
	    CFSTR("PowerOffDisabled"), CFSTR("RetriesUntilHint"), CFSTR("showInputMenu") };
	CFPropertyListRef changes = data != NULL ? CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListImmutable, NULL, NULL) : NULL;
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)LOGIN_PREFS,
	    (CFIndex)strlen(LOGIN_PREFS), false);
	CFMutableDictionaryRef prefs = NULL;
	CFDataRef old = NULL;
	uint32_t error = 0;

	if (changes == NULL || CFGetTypeID(changes) != CFDictionaryGetTypeID()) {
		error = kODErrorRecordParameterError;
		goto out;
	}
	{
		FILE *f = fopen(LOGIN_PREFS, "r");
		if (f != NULL) {
			CFMutableDataRef bytes = CFDataCreateMutable(NULL, 0);
			UInt8 buf[4096];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
				CFDataAppendBytes(bytes, buf, (CFIndex)n);
			fclose(f);
			old = bytes;
		}
	}
	if (old != NULL) {
		CFPropertyListRef p = CFPropertyListCreateWithData(NULL, old,
		    kCFPropertyListMutableContainers, NULL, NULL);
		if (p != NULL && CFGetTypeID(p) == CFDictionaryGetTypeID())
			prefs = (CFMutableDictionaryRef)p;
		else if (p != NULL)
			CFRelease(p);
	}
	if (prefs == NULL)
		prefs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
	{
	CFIndex n = CFDictionaryGetCount(changes);
	const void *keys[n > 0 ? n : 1], *values[n > 0 ? n : 1];
	CFDictionaryGetKeysAndValues(changes, keys, values);
	for (CFIndex i = 0; i < n; i++) {
		bool ok = false;
		for (size_t a = 0; a < sizeof(allowed) / sizeof(allowed[0]); a++)
			if (CFEqual(keys[i], allowed[a]))
				ok = true;
		if (!ok) {
			error = kODErrorRecordAttributeUnknownType;
			goto out;
		}
		if (values[i] == kCFNull || (CFGetTypeID(values[i]) == CFStringGetTypeID() &&
		    CFStringGetLength(values[i]) == 0))
			CFDictionaryRemoveValue(prefs, keys[i]);
		else
			CFDictionarySetValue(prefs, keys[i], values[i]);
	}
	}
	{
		CFDataRef xml = CFPropertyListCreateData(NULL, prefs, kCFPropertyListXMLFormat_v1_0, 0, NULL);
		char tmp[] = LOGIN_PREFS ".XXXXXX";
		int fd = xml != NULL ? mkstemp(tmp) : -1;
		if (fd < 0 || write(fd, CFDataGetBytePtr(xml), (size_t)CFDataGetLength(xml)) !=
		    CFDataGetLength(xml) || fchmod(fd, 0644) != 0 || close(fd) != 0 ||
		    rename(tmp, LOGIN_PREFS) != 0) {
			if (fd >= 0)
				unlink(tmp);
			error = kODErrorDaemonError;
		}
		if (xml != NULL)
			CFRelease(xml);
	}
out:
	if (changes != NULL)
		CFRelease(changes);
	if (prefs != NULL)
		CFRelease(prefs);
	if (old != NULL)
		CFRelease(old);
	if (url != NULL)
		CFRelease(url);
	return (error);
}

/* The home of a user that no longer exists, /Users/NAME only. */
static uint32_t
remove_home(const char *name)
{
	char home[PATH_MAX];

	if (name == NULL || name[0] == '\0' || name[0] == '.' || strchr(name, '/') != NULL)
		return (kODErrorRecordParameterError);
	if (getpwnam(name) != NULL)
		return (kODErrorRecordPermissionError);
	snprintf(home, sizeof(home), "/Users/%s", name);
	char *argv[] = { "/bin/rm", "-rf", home, NULL };
	return (run(argv) == 0 ? 0 : kODErrorDaemonError);
}

/* The system's time zone: /etc/localtime points at the zone (one of
   zoneinfo's files, no ".." in its name), and the change is posted. */
static uint32_t
set_time_zone(const char *zone)
{
	char target[PATH_MAX];
	struct stat st;

	if (zone == NULL || zone[0] == '\0' || zone[0] == '/' || strstr(zone, "..") != NULL)
		return (kODErrorRecordParameterError);
	snprintf(target, sizeof(target), "%s/%s", ZONEINFO, zone);
	if (stat(target, &st) != 0 || !S_ISREG(st.st_mode))
		return (kODErrorRecordAttributeValueNotFound);
	unlink(LOCALTIME ".new");
	if (symlink(target, LOCALTIME ".new") != 0 || rename(LOCALTIME ".new", LOCALTIME) != 0)
		return (kODErrorDaemonError);
	notify_post("org.minkar.timezone");
	notify_post("com.apple.system.timezone");
	return (0);
}

/* ---- Requests ---------------------------------------------------------- */

static void
reply(xpc_connection_t peer, uint64_t clientID, CFStringRef funcname, CFIndex count,
    const CFTypeRef *values, uint32_t error, bool complete)
{
	CFDictionaryRef result = schema_construct_result(funcname, count, values);
	CFDataRef data = CFPropertyListCreateData(NULL, result, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);

	xpc_dictionary_set_uint64(msg, kODXPCReplyClientID, clientID);
	if (data != NULL)
		xpc_dictionary_set_data(msg, kODXPCReplyData, CFDataGetBytePtr(data),
		    (size_t)CFDataGetLength(data));
	xpc_dictionary_set_uint64(msg, kODXPCReplyError, error);
	xpc_dictionary_set_bool(msg, kODXPCReplyComplete, complete);
	xpc_connection_send_message(peer, msg);
	xpc_release(msg);
	if (data != NULL)
		CFRelease(data);
	CFRelease(result);
}

/* A simple answer: values, then the error's description (NULL). */
#define ANSWER(...) do { \
	CFTypeRef _v[] = { __VA_ARGS__ }; \
	reply(peer, clientID, func, sizeof(_v) / sizeof(_v[0]), _v, error, true); \
	} while (0)

static bool
node_name_known(CFStringRef name)
{
	return (CFEqual(name, LOCAL_NODE) || CFEqual(name, SEARCH_NODE) ||
	    CFEqual(name, CONTACTS_NODE));
}

/* Whether the request may change user's account. */
static bool
may_edit(xpc_connection_t peer, struct object *node, const char *user)
{
	(void)user;
	if (xpc_connection_get_euid(peer) == 0)
		return (true);
	return (node != NULL && node->authUser != NULL && is_admin(node->authUser));
}

static void
handle(xpc_connection_t peer, xpc_object_t msg)
{
	uint64_t clientID = xpc_dictionary_get_uint64(msg, kODXPCMessageClientID);
	const uint8_t *sessionUUID = xpc_dictionary_get_uuid(msg, kODXPCMessageSession);
	const uint8_t *nodeUUID = xpc_dictionary_get_uuid(msg, kODXPCMessageNode);
	size_t len = 0;
	const void *bytes = xpc_dictionary_get_data(msg, kODXPCMessageRequest, &len);
	CFDataRef data;
	CFPropertyListRef request = NULL;
	CFStringRef func = NULL;
	CFArrayRef args = NULL;
	struct object *node = object_find(nodeUUID, OBJ_NODE);
	uint32_t error = 0;

	if (bytes != NULL && (data = CFDataCreate(NULL, bytes, (CFIndex)len)) != NULL) {
		request = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
		CFRelease(data);
	}
	schema_deconstruct_result(request, &func, &args);
	if (func == NULL) {
		CFStringRef unknown = CFSTR("ODUnknown");
		reply(peer, clientID, unknown, 0, NULL, kODErrorRecordParameterError, true);
		goto out;
	}
#define ARG(i)	schema_get_value_at_index(args, (i))
#define IS(name) CFEqual(func, CFSTR(name))

	if (IS("ODSessionCreate")) {
		struct object *s = object_new(OBJ_SESSION, peer);
		CFDataRef u = uuid_data(s->uuid);
		ANSWER(u, NULL);
		CFRelease(u);
	} else if (IS("ODSessionRelease")) {
		struct object *s = object_find(sessionUUID, OBJ_SESSION);
		if (s != NULL)
			object_free(s);
		ANSWER(NULL);
	} else if (IS("ODSessionCopyNodeNames")) {
		CFArrayRef names = CFArrayCreate(NULL, (const void *[]){ SEARCH_NODE, CONTACTS_NODE,
		    LOCAL_NODE }, 3, &kCFTypeArrayCallBacks);
		ANSWER(names, NULL);
		CFRelease(names);
	} else if (IS("ODNodeCreateWithName")) {
		CFStringRef name = ARG(0);
		if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID() || !node_name_known(name)) {
			error = kODErrorNodeUnknownName;
			ANSWER(NULL, NULL);
		} else {
			struct object *n = object_new(OBJ_NODE, peer);
			n->name = CFRetain(name);
			CFDataRef u = uuid_data(n->uuid);
			ANSWER(u, NULL);
			CFRelease(u);
		}
	} else if (IS("ODNodeRelease")) {
		if (node != NULL)
			object_free(node);
		ANSWER(NULL);
	} else if (node == NULL) {
		/* Everything else is asked of a node. */
		error = kODErrorNodeInvalid;
		ANSWER(NULL, NULL, NULL, NULL);
	} else if (IS("ODNodeCopySubnodeNames") || IS("ODNodeCopyUnreachableSubnodeNames")) {
		CFArrayRef none = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
		ANSWER(none, NULL);
		CFRelease(none);
	} else if (IS("ODNodeCopyDetails")) {
		CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFArrayRef n = CFArrayCreate(NULL, (const void **)&node->name, 1, &kCFTypeArrayCallBacks);
		CFDictionarySetValue(d, A("NodePath"), n);
		CFRelease(n);
		ANSWER(d, NULL);
		CFRelease(d);
	} else if (IS("ODNodeCopySupportedRecordTypes")) {
		CFArrayRef types = CFArrayCreate(NULL, (const void *[]){ REC_USERS, REC_GROUPS }, 2,
		    &kCFTypeArrayCallBacks);
		ANSWER(types, NULL);
		CFRelease(types);
	} else if (IS("ODNodeCopySupportedAttributes")) {
		CFArrayRef attrs = CFArrayCreate(NULL, (const void *[]){ A("RecordName"),
		    A("UniqueID"), A("PrimaryGroupID"), A("RealName"), A("NFSHomeDirectory"),
		    A("UserShell"), A("GeneratedUID"), A("GroupMembership") }, 8,
		    &kCFTypeArrayCallBacks);
		ANSWER(attrs, NULL);
		CFRelease(attrs);
	} else if (IS("ODNodeSetCredentials")) {
		char *user = cstring(ARG(1)), *password = cstring(ARG(2));
		error = password_check(user, password);
		if (error == 0) {
			free(node->authUser);
			node->authUser = user;
			user = NULL;
		}
		free(user);
		free(password);
		ANSWER(NULL, NULL);
	} else if (IS("ODRecordVerifyPassword")) {
		char *user = cstring(ARG(1)), *password = cstring(ARG(3));
		error = password_check(user, password);
		free(user);
		free(password);
		ANSWER(NULL, NULL);
	} else if (IS("ODRecordChangePassword")) {
		char *user = cstring(ARG(1)), *old = cstring(ARG(3)), *new = cstring(ARG(4));
		char *hash;
		if (user == NULL || new == NULL)
			error = kODErrorCredentialsParameterError;
		else if (!may_edit(peer, node, user) && (error = password_check(user, old)) != 0)
			;
		else if ((hash = hash_password(new)) == NULL)
			error = kODErrorCredentialsOperationFailed;
		else {
			error = account_update(user, ^(struct passwd *pw) { pw->pw_passwd = hash; });
			free(hash);
		}
		free(user);
		free(old);
		free(new);
		ANSWER(NULL, NULL);
	} else if (IS("ODRecordSetValue")) {
		CFStringRef type = ARG(0), attr = ARG(3);
		CFArrayRef values = as_array(ARG(4));
		char *user = cstring(ARG(1)), *value = NULL;
		if (values != NULL && CFArrayGetCount(values) > 0)
			value = cstring(CFArrayGetValueAtIndex(values, 0));
		if (type == NULL || !CFEqual(type, REC_USERS) || attr == NULL || user == NULL || value == NULL)
			error = kODErrorPluginOperationNotSupported;
		else if (!may_edit(peer, node, user))
			error = kODErrorRecordPermissionError;
		else if (CFEqual(attr, A("RealName")))
			error = account_update(user, ^(struct passwd *pw) { pw->pw_gecos = value; });
		else if (CFEqual(attr, A("UserShell")))
			error = account_update(user, ^(struct passwd *pw) { pw->pw_shell = value; });
		else
			error = kODErrorPluginOperationNotSupported;
		if (values != NULL)
			CFRelease(values);
		free(user);
		free(value);
		ANSWER(NULL, NULL);
	} else if (IS("ODNodeCreateRecord")) {
		CFStringRef type = ARG(0);
		CFDictionaryRef attrs = ARG(2);
		char *name = cstring(ARG(1)), *full = NULL;
		CFDictionaryRef made = NULL;

		if (attrs != NULL && CFGetTypeID(attrs) == CFDictionaryGetTypeID()) {
			CFArrayRef v = as_array(CFDictionaryGetValue(attrs, A("RealName")));
			if (v != NULL && CFArrayGetCount(v) > 0)
				full = cstring(CFArrayGetValueAtIndex(v, 0));
			if (v != NULL)
				CFRelease(v);
		}
		if (type == NULL || !CFEqual(type, REC_USERS) || name == NULL)
			error = kODErrorPluginOperationNotSupported;
		else if (!may_edit(peer, node, name))
			error = kODErrorRecordPermissionError;
		else if (getpwnam(name) != NULL)
			error = kODErrorRecordAlreadyExists;
		else {
			/* A password nobody knows until the pane sets the user's. */
			char password[33];
			static const char chars[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
			for (int i = 0; i < 32; i++)
				password[i] = chars[arc4random_uniform(sizeof(chars) - 1)];
			password[32] = '\0';
			char *argv[] = { SYSADMINCTL, "-addUser", name, "-fullName", full != NULL ? full : name,
			    "-password", password, NULL };
			struct passwd *pw;
			if (run(argv) != 0 || (pw = getpwnam(name)) == NULL)
				error = kODErrorDaemonError;
			else
				made = user_record(pw);
		}
		ANSWER(NULL, made);
		if (made != NULL)
			CFRelease(made);
		free(name);
		free(full);
	} else if (IS("ODRecordDelete")) {
		CFStringRef type = ARG(0);
		char *name = cstring(ARG(1));
		struct passwd *pw = name != NULL ? getpwnam(name) : NULL;

		if (type == NULL || !CFEqual(type, REC_USERS) || name == NULL)
			error = kODErrorPluginOperationNotSupported;
		else if (!may_edit(peer, node, name))
			error = kODErrorRecordPermissionError;
		else if (pw == NULL)
			error = kODErrorRecordNoLongerExists;
		else if (pw->pw_uid < 500 || (node != NULL && node->authUser != NULL &&
		    strcmp(node->authUser, name) == 0))
			/* Not a system account, nor the administrator deleting. */
			error = kODErrorRecordPermissionError;
		else {
			char *argv[] = { SYSADMINCTL, "-deleteUser", name, "-keepHome", NULL };
			if (run(argv) != 0)
				error = kODErrorDaemonError;
		}
		free(name);
		ANSWER(NULL);
	} else if (IS("ODRecordAddValue") || IS("ODRecordRemoveValue")) {
		CFStringRef type = ARG(0), attr = ARG(3);
		char *group = cstring(ARG(1)), *member = cstring(ARG(4));

		if (type == NULL || !CFEqual(type, REC_GROUPS) || attr == NULL ||
		    !CFEqual(attr, A("GroupMembership")) || group == NULL || member == NULL)
			error = kODErrorPluginOperationNotSupported;
		else if (!may_edit(peer, node, member))
			error = kODErrorRecordPermissionError;
		else if (getpwnam(member) == NULL)
			error = kODErrorRecordAttributeValueNotFound;
		else if (IS("ODRecordRemoveValue") && strcmp(group, "admin") == 0 && node != NULL &&
		    node->authUser != NULL && strcmp(node->authUser, member) == 0)
			/* An administrator does not take itself out of admin. */
			error = kODErrorRecordPermissionError;
		else
			error = group_edit(group, member, IS("ODRecordAddValue"));
		free(group);
		free(member);
		ANSWER(NULL);
	} else if (IS("ODNodeCustomCall")) {
		CFNumberRef codeNum = ARG(0);
		CFTypeRef sent = ARG(1);
		CFIndex code = 0;

		if (codeNum != NULL)
			CFNumberGetValue(codeNum, kCFNumberCFIndexType, &code);
		if (!may_edit(peer, node, NULL))
			error = kODErrorRecordPermissionError;
		else if (code == kCustomLoginOptions && sent != NULL && CFGetTypeID(sent) == CFDataGetTypeID())
			error = login_options(sent);
		else if (code == kCustomRemoveHome && sent != NULL && CFGetTypeID(sent) == CFDataGetTypeID()) {
			char name[256] = "";
			CFIndex len = CFDataGetLength(sent);
			if (len > 0 && len < (CFIndex)sizeof(name))
				memcpy(name, CFDataGetBytePtr(sent), (size_t)len);
			error = remove_home(name);
		} else if (code == kCustomTimeZone && sent != NULL && CFGetTypeID(sent) == CFDataGetTypeID()) {
			char zone[256] = "";
			CFIndex len = CFDataGetLength(sent);
			if (len > 0 && len < (CFIndex)sizeof(zone))
				memcpy(zone, CFDataGetBytePtr(sent), (size_t)len);
			error = set_time_zone(zone);
		} else
			error = kODErrorPluginOperationNotSupported;
		ANSWER(NULL, NULL);
	} else if (IS("ODQueryCreateWithNode")) {
		CFArrayRef types = as_array(ARG(0)), wanted = as_array(ARG(3)), attrs = as_array(ARG(4));
		CFStringRef attr = ARG(1);
		CFNumberRef matchNum = ARG(2), maxNum = ARG(5);
		CFIndex match = 0, max = 0;
		uuid_t q;
		CFDataRef qid;
		CFArrayRef found;

		if (matchNum != NULL)
			CFNumberGetValue(matchNum, kCFNumberCFIndexType, &match);
		if (maxNum != NULL)
			CFNumberGetValue(maxNum, kCFNumberCFIndexType, &max);
		uuid_generate_random(q);
		qid = uuid_data(q);
		found = records_find(types, attr, (uint32_t)match, wanted, attrs, max, &error);
		{
			CFTypeRef v[] = { qid, NULL };
			reply(peer, clientID, func, 2, v, error, error != 0);
		}
		if (error == 0) {
			CFIndex n = CFArrayGetCount(found);
			for (CFIndex i = 0; i < n; i++) {
				CFTypeRef v[] = { qid, CFArrayGetValueAtIndex(found, i), NULL };
				reply(peer, clientID, CFSTR("ODQueryResponse"), 3, v, 0, i == n - 1);
			}
			if (n == 0) {
				CFTypeRef v[] = { qid, NULL, NULL };
				reply(peer, clientID, CFSTR("ODQueryResponse"), 3, v, 0, true);
			}
		}
		CFRelease(found);
		CFRelease(qid);
		if (types != NULL)
			CFRelease(types);
		if (wanted != NULL)
			CFRelease(wanted);
		if (attrs != NULL)
			CFRelease(attrs);
	} else if (IS("ODQuerySynchronize")) {
		/* Queries end once answered: the framework makes it again. */
		error = kODErrorQueryInvalid;
		ANSWER(NULL);
	} else if (IS("ODQueryCancel") || IS("ODContextRelease")) {
		ANSWER(NULL);
	} else if (IS("ODNodeVerifyCredentialsExtended") || IS("ODNodeSetCredentialsExtended") ||
	    IS("ODRecordVerifyPasswordExtended")) {
		error = kODErrorCredentialsMethodNotSupported;
		ANSWER(NULL, NULL, NULL, NULL);
	} else {
		error = kODErrorPluginOperationNotSupported;
		ANSWER(NULL, NULL, NULL, NULL);
	}
#undef ARG
#undef IS
out:
	if (func != NULL)
		CFRelease(func);
	if (args != NULL)
		CFRelease(args);
	if (request != NULL)
		CFRelease(request);
}

void
odapi_start(void)
{
	xpc_connection_t listener;

	queue = dispatch_queue_create("org.minkar.directoryd.api", DISPATCH_QUEUE_SERIAL);
	listener = xpc_connection_create_mach_service(kODAPIPortName, queue,
	    XPC_CONNECTION_MACH_SERVICE_LISTENER);
	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
		if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
			return;
		xpc_connection_set_target_queue(peer, queue);
		xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
			if (xpc_get_type(event) == XPC_TYPE_DICTIONARY)
				handle(peer, event);
			else if (xpc_get_type(event) == XPC_TYPE_ERROR)
				objects_drop(peer);
		});
		xpc_connection_resume(peer);
	});
	xpc_connection_resume(listener);
}
