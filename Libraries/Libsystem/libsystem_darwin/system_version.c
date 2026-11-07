/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * os_system_version_get_current_version: the version programs compare
 * against, ProductVersion in SystemVersion.plist (the Cocoa API version:
 * 10.15 for API 19), as NSProcessInfo's operatingSystemVersion gives it.
 * The Swift runtime asks for it.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <xpc/xpc.h>

xpc_object_t xpc_create_from_plist(uint8_t *buf, size_t length);

struct os_system_version_s {
	unsigned int major;
	unsigned int minor;
	unsigned int patch;
};

int os_system_version_get_current_version(struct os_system_version_s *version);

#define kSystemVersion	"/System/Library/CoreServices/SystemVersion.plist"

int
os_system_version_get_current_version(struct os_system_version_s *version)
{
	struct stat sb;
	uint8_t *buf;
	int fd, err = ENOENT;

	if (version == NULL)
		return (EINVAL);
	version->major = version->minor = version->patch = 0;
	fd = open(kSystemVersion, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return (errno);
	if (fstat(fd, &sb) != 0 || sb.st_size <= 0 || sb.st_size > 65536) {
		close(fd);
		return (EINVAL);
	}
	buf = malloc((size_t)sb.st_size);
	if (buf != NULL && read(fd, buf, (size_t)sb.st_size) == sb.st_size) {
		xpc_object_t plist = xpc_create_from_plist(buf, (size_t)sb.st_size);
		const char *v = plist != NULL && xpc_get_type(plist) == XPC_TYPE_DICTIONARY ?
		    xpc_dictionary_get_string(plist, "ProductVersion") : NULL;
		if (v != NULL && sscanf(v, "%u.%u.%u", &version->major, &version->minor,
		    &version->patch) >= 1)
			err = 0;
		if (plist != NULL)
			xpc_release(plist);
	}
	free(buf);
	close(fd);
	return (err);
}
