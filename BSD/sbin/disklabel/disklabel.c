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
 * disklabel: Apple labels. A label is a 512-byte header at the start of a
 * device (struct applelabel, big-endian: the magic, the offset and size of
 * a property area, its crc32) and the property area, a property list. The
 * kernel's label scheme looks for one on a partition of type Apple_Label
 * and publishes the labeled content as a child medium, from the "Base"
 * (where it starts) and "Size" properties; the others (owner, mode, the
 * content's hint...) are the label's. -create writes a label, -properties
 * shows and changes its properties, -status checks it.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/storage/IOAppleLabelScheme.h>

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define HEADER_SIZE	512
#define AREA_MAX	131072		/* the most the kernel reads */
#define AREA_DEFAULT	(64 * 1024)
#define ALIGN		4096		/* where the content may start */

struct device {
	const char	*path;
	int		 fd;
	uint32_t	 bsize;
	uint64_t	 size;		/* bytes */
};

static void
open_device(struct device *d, const char *name, bool write)
{
	char raw[PATH_MAX];
	uint64_t count = 0;

	/* /dev/diskNsM -> /dev/rdiskNsM, and a bare diskNsM too. */
	if (strncmp(name, "/dev/disk", 9) == 0)
		snprintf(raw, sizeof(raw), "/dev/r%s", name + 5);
	else if (strncmp(name, "disk", 4) == 0)
		snprintf(raw, sizeof(raw), "/dev/r%s", name);
	else
		strlcpy(raw, name, sizeof(raw));
	d->path = strdup(raw);
	if ((d->fd = open(raw, write ? O_RDWR : O_RDONLY)) < 0)
		err(1, "%s", raw);
	if (ioctl(d->fd, DKIOCGETBLOCKSIZE, &d->bsize) != 0 ||
	    ioctl(d->fd, DKIOCGETBLOCKCOUNT, &count) != 0) {
		struct stat st;

		/* A file (an image) works too. */
		if (fstat(d->fd, &st) != 0 || !S_ISREG(st.st_mode))
			err(1, "%s: not a disk", raw);
		d->bsize = 512;
		d->size = (uint64_t)st.st_size;
		return;
	}
	d->size = count * d->bsize;
}

/* Reads [off, off + len) whole blocks around it, into buf (len bytes). */
static bool
read_bytes(struct device *d, uint64_t off, void *buf, size_t len)
{
	uint64_t start = off / d->bsize * d->bsize;
	size_t span = (size_t)((off + len + d->bsize - 1) / d->bsize * d->bsize -
	    start);
	char *tmp = malloc(span);
	bool ok;

	if (tmp == NULL)
		err(1, NULL);
	ok = pread(d->fd, tmp, span, (off_t)start) == (ssize_t)span;
	if (ok)
		memcpy(buf, tmp + (off - start), len);
	free(tmp);
	return (ok);
}

static bool
write_bytes(struct device *d, uint64_t off, const void *buf, size_t len)
{
	uint64_t start = off / d->bsize * d->bsize;
	size_t span = (size_t)((off + len + d->bsize - 1) / d->bsize * d->bsize -
	    start);
	char *tmp = malloc(span);
	bool ok;

	if (tmp == NULL)
		err(1, NULL);
	/* Keep what shares the first and last blocks. */
	if (pread(d->fd, tmp, span, (off_t)start) != (ssize_t)span)
		memset(tmp, 0, span);
	memcpy(tmp + (off - start), buf, len);
	ok = pwrite(d->fd, tmp, span, (off_t)start) == (ssize_t)span;
	free(tmp);
	return (ok);
}

/* ---- The label ------------------------------------------------------------ */

/*
 * The label's properties, or NULL with why in *why. The header's fields
 * are big-endian; the checksum is zlib's crc32 of the whole area.
 */
static CFMutableDictionaryRef
read_label(struct device *d, struct applelabel *h, const char **why)
{
	CFDataRef data;
	CFPropertyListRef pl;
	uint32_t size;
	uint64_t off;
	char *area;

	if (!read_bytes(d, 0, h, sizeof(*h))) {
		*why = "can't read the header";
		return (NULL);
	}
	if (OSSwapBigToHostInt16(h->al_magic) != AL_MAGIC) {
		*why = "no Apple label";
		return (NULL);
	}
	if (OSSwapBigToHostInt16(h->al_type) != AL_TYPE_DEFAULT) {
		*why = "an unknown label version";
		return (NULL);
	}
	off = OSSwapBigToHostInt64(h->al_offset);
	size = OSSwapBigToHostInt32(h->al_size);
	if (size == 0 || size > AREA_MAX || off + size > d->size) {
		*why = "the property area is out of bounds";
		return (NULL);
	}
	if ((area = malloc(size)) == NULL)
		err(1, NULL);
	if (!read_bytes(d, off, area, size)) {
		free(area);
		*why = "can't read the property area";
		return (NULL);
	}
	if ((uint32_t)crc32(0, (const Bytef *)area, size) !=
	    OSSwapBigToHostInt32(h->al_checksum)) {
		free(area);
		*why = "the checksum is wrong";
		return (NULL);
	}
	data = CFDataCreate(NULL, (const UInt8 *)area, (CFIndex)strnlen(area,
	    size));
	free(area);
	pl = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
	CFRelease(data);
	if (pl == NULL || CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
		if (pl != NULL)
			CFRelease(pl);
		*why = "the properties are not a dictionary";
		return (NULL);
	}
	return ((CFMutableDictionaryRef)pl);
}

/* The properties into an area of size bytes at off, and the header. */
static bool
write_label(struct device *d, CFDictionaryRef props, uint64_t off,
    uint32_t size)
{
	struct applelabel h;
	CFDataRef data = CFPropertyListCreateData(NULL, props,
	    kCFPropertyListXMLFormat_v1_0, 0, NULL);
	char *area;
	bool ok;

	if (data == NULL)
		return (false);
	if ((size_t)CFDataGetLength(data) >= size) {
		warnx("the properties need more than %u bytes (-msize)", size);
		CFRelease(data);
		return (false);
	}
	if ((area = calloc(1, size)) == NULL)
		err(1, NULL);
	memcpy(area, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
	CFRelease(data);
	memset(&h, 0, sizeof(h));
	h.al_magic = OSSwapHostToBigInt16(AL_MAGIC);
	h.al_type = OSSwapHostToBigInt16(AL_TYPE_DEFAULT);
	h.al_flags = OSSwapHostToBigInt32(AL_FLAG_DEFAULT);
	h.al_offset = OSSwapHostToBigInt64(off);
	h.al_size = OSSwapHostToBigInt32(size);
	h.al_checksum = OSSwapHostToBigInt32((uint32_t)crc32(0,
	    (const Bytef *)area, size));
	/* The area first: a header never points at an unwritten one. */
	ok = write_bytes(d, off, area, size) &&
	    write_bytes(d, 0, &h, sizeof(h)) && fsync(d->fd) == 0;
	free(area);
	return (ok);
}

/* ---- Properties ------------------------------------------------------------ */

/* "name=value": a number when it is one (0x hex, a leading 0 octal, as
 * modes are written), a boolean for true/false, else a string. */
static bool
set_property(CFMutableDictionaryRef props, const char *arg)
{
	const char *eq = strchr(arg, '=');
	CFStringRef key;
	CFTypeRef value;
	char *end;
	long long n;

	if (eq == NULL || eq == arg)
		return (false);
	key = CFStringCreateWithBytes(NULL, (const UInt8 *)arg, eq - arg,
	    kCFStringEncodingUTF8, false);
	eq++;
	errno = 0;
	n = strtoll(eq, &end, 0);
	if (*eq != '\0' && *end == '\0' && errno == 0)
		value = CFNumberCreate(NULL, kCFNumberLongLongType, &n);
	else if (strcmp(eq, "true") == 0)
		value = CFRetain(kCFBooleanTrue);
	else if (strcmp(eq, "false") == 0)
		value = CFRetain(kCFBooleanFalse);
	else
		value = CFStringCreateWithCString(NULL, eq, kCFStringEncodingUTF8);
	CFDictionarySetValue(props, key, value);
	CFRelease(key);
	CFRelease(value);
	return (true);
}

static void
print_value(CFTypeRef v)
{
	char buf[1024];

	if (CFGetTypeID(v) == CFStringGetTypeID()) {
		CFStringGetCString(v, buf, sizeof(buf), kCFStringEncodingUTF8);
		printf("%s", buf);
	} else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
		long long n = 0;

		CFNumberGetValue(v, kCFNumberLongLongType, &n);
		printf("%lld", n);
	} else if (CFGetTypeID(v) == CFBooleanGetTypeID()) {
		printf("%s", CFBooleanGetValue(v) ? "true" : "false");
	} else {
		printf("(complex value)");
	}
}

static void
print_property(CFStringRef key, CFTypeRef v)
{
	char buf[256];

	CFStringGetCString(key, buf, sizeof(buf), kCFStringEncodingUTF8);
	printf("%s: ", buf);
	print_value(v);
	printf("\n");
}

static void
print_properties(CFDictionaryRef props)
{
	CFIndex n = CFDictionaryGetCount(props), i;
	const void **keys = calloc((size_t)n + 1, sizeof(*keys));
	CFMutableArrayRef sorted;

	CFDictionaryGetKeysAndValues(props, keys, NULL);
	sorted = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (i = 0; i < n; i++)
		CFArrayAppendValue(sorted, keys[i]);
	CFArraySortValues(sorted, CFRangeMake(0, n),
	    (CFComparatorFunction)CFStringCompare, NULL);
	for (i = 0; i < n; i++) {
		CFStringRef k = CFArrayGetValueAtIndex(sorted, i);

		print_property(k, CFDictionaryGetValue(props, k));
	}
	CFRelease(sorted);
	free(keys);
}

/* ---- Commands ---------------------------------------------------------------- */

static int
create(const char *dev, int argc, char **argv)
{
	struct device d;
	CFMutableDictionaryRef props;
	uint64_t base, size;
	uint32_t msize = AREA_DEFAULT;
	CFNumberRef num;
	int i;

	props = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	for (i = 0; i < argc; i++) {
		if (strncmp(argv[i], "-msize=", 7) == 0) {
			char *end;
			unsigned long long v = strtoull(argv[i] + 7, &end, 0);

			if (*end == 'k' || *end == 'K')
				v *= 1024, end++;
			if (*end != '\0' || v < 512 || v > AREA_MAX)
				errx(1, "-msize: from 512 to %d bytes", AREA_MAX);
			msize = (uint32_t)v;
		} else if (!set_property(props, argv[i])) {
			errx(1, "%s: not a property=value", argv[i]);
		}
	}
	open_device(&d, dev, true);
	/* The content starts after the header and the area, aligned. */
	base = (HEADER_SIZE + msize + ALIGN - 1) / ALIGN * ALIGN;
	if (base >= d.size)
		errx(1, "%s: too small for a label", d.path);
	size = d.size - base;
	num = CFNumberCreate(NULL, kCFNumberSInt64Type, &base);
	CFDictionarySetValue(props, CFSTR("Base"), num);
	CFRelease(num);
	num = CFNumberCreate(NULL, kCFNumberSInt64Type, &size);
	CFDictionarySetValue(props, CFSTR("Size"), num);
	CFRelease(num);
	if (!write_label(&d, props, HEADER_SIZE, msize))
		err(1, "%s: can't write the label", d.path);
	close(d.fd);
	CFRelease(props);
	return (0);
}

static int
properties(const char *dev, int argc, char **argv)
{
	struct device d;
	struct applelabel h;
	CFMutableDictionaryRef props;
	const char *why = NULL;
	bool changes = false;
	int i, rc = 0;

	for (i = 0; i < argc; i++)
		if (strchr(argv[i], '=') != NULL)
			changes = true;
	open_device(&d, dev, changes);
	if ((props = read_label(&d, &h, &why)) == NULL)
		errx(1, "%s: %s", d.path, why);
	if (argc == 0)
		print_properties(props);
	for (i = 0; i < argc; i++) {
		if (strchr(argv[i], '=') != NULL) {
			if (strncmp(argv[i], "Base=", 5) == 0 ||
			    strncmp(argv[i], "Size=", 5) == 0)
				errx(1, "%s: set by the label itself", argv[i]);
			if (!set_property(props, argv[i]))
				errx(1, "%s: not a property=value", argv[i]);
		} else {
			CFStringRef k = CFStringCreateWithCString(NULL, argv[i],
			    kCFStringEncodingUTF8);
			CFTypeRef v = CFDictionaryGetValue(props, k);

			if (v != NULL)
				print_property(k, v);
			else {
				warnx("%s: no such property", argv[i]);
				rc = 1;
			}
			CFRelease(k);
		}
	}
	if (changes && !write_label(&d, props,
	    OSSwapBigToHostInt64(h.al_offset), OSSwapBigToHostInt32(h.al_size)))
		err(1, "%s: can't write the label", d.path);
	close(d.fd);
	CFRelease(props);
	return (rc);
}

static int
status(const char *dev)
{
	struct device d;
	struct applelabel h;
	CFMutableDictionaryRef props;
	const char *why = NULL;

	open_device(&d, dev, false);
	if ((props = read_label(&d, &h, &why)) == NULL) {
		printf("%s: %s\n", d.path, why);
		return (1);
	}
	printf("%s: a valid Apple label\n", d.path);
	printf("Property area: %u bytes at offset %llu, checksum 0x%08x\n",
	    OSSwapBigToHostInt32(h.al_size),
	    (unsigned long long)OSSwapBigToHostInt64(h.al_offset),
	    OSSwapBigToHostInt32(h.al_checksum));
	print_properties(props);
	close(d.fd);
	CFRelease(props);
	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: disklabel -create device [-msize=size] [property=value] ...\n"
	    "       disklabel -properties device [property[=value]] ...\n"
	    "       disklabel -status device\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc < 3)
		usage();
	if (strcmp(argv[1], "-create") == 0)
		return (create(argv[2], argc - 3, argv + 3));
	if (strcmp(argv[1], "-properties") == 0)
		return (properties(argv[2], argc - 3, argv + 3));
	if (strcmp(argv[1], "-status") == 0 && argc == 3)
		return (status(argv[2]));
	usage();
}
