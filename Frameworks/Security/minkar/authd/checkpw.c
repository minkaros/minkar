/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * checkpw_internal, which authd (APSL) calls to check a user's password: the
 * user's record in the local directory (OpenDirectory's /Local/Default, as
 * the login window checks it) verifies it.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CFOpenDirectory/CFOpenDirectory.h>
#include <Security/checkpw.h>
#include <pwd.h>

int checkpw_internal(const struct passwd *pw, const char *password);

int checkpw_internal(const struct passwd *pw, const char *password)
{
	CFStringRef name, secret;
	ODNodeRef node;
	ODRecordRef record;
	int result;

	if (pw == NULL || pw->pw_name == NULL || password == NULL)
		return CHECKPW_FAILURE;
	node = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault, CFSTR("/Local/Default"), NULL);
	if (node == NULL)
		return CHECKPW_FAILURE;
	name = CFStringCreateWithCString(kCFAllocatorDefault, pw->pw_name, kCFStringEncodingUTF8);
	record = name != NULL ? ODNodeCopyRecord(node, kODRecordTypeUsers, name, NULL, NULL) : NULL;
	if (record == NULL)
		result = CHECKPW_UNKNOWNUSER;
	else {
		secret = CFStringCreateWithCString(kCFAllocatorDefault, password, kCFStringEncodingUTF8);
		result = secret != NULL && ODRecordVerifyPassword(record, secret, NULL) ? CHECKPW_SUCCESS : CHECKPW_BADPASSWORD;
		if (secret != NULL)
			CFRelease(secret);
		CFRelease(record);
	}
	if (name != NULL)
		CFRelease(name);
	CFRelease(node);
	return result;
}
