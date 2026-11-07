/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * _dyld_is_objc_constant: whether an address is an Objective-C constant
 * of a loaded image (a constant string in a __cfstring section), so the
 * Swift runtime and Foundation can treat it as immortal. The sections are
 * collected as images are added and dropped as they are removed.
 */

#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <os/lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum {
	dyld_objc_string_kind
} DyldObjCConstantKind;

bool _dyld_is_objc_constant(DyldObjCConstantKind kind, const void *addr);

typedef struct {
	const struct mach_header *mh;
	uintptr_t start, end;
} Range;

static os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
static Range *ranges;
static size_t count, capacity;
static bool registered;

static void
addRange(const struct mach_header *mh, uintptr_t start, uintptr_t end)
{
	if (count == capacity) {
		size_t cap = capacity ? capacity * 2 : 64;
		Range *grown = realloc(ranges, cap * sizeof(Range));
		if (grown == NULL)
			return;
		ranges = grown;
		capacity = cap;
	}
	ranges[count++] = (Range){ mh, start, end };
}

static void
added(const struct mach_header *mh, intptr_t slide)
{
	static const char *const segments[] = { "__DATA_CONST", "__DATA", "__DATA_DIRTY" };
	unsigned long size;

	(void)slide;
	os_unfair_lock_lock(&lock);
	for (unsigned i = 0; i < sizeof(segments) / sizeof(segments[0]); i++) {
		uint8_t *p = getsectiondata((const struct mach_header_64 *)mh, segments[i],
		    "__cfstring", &size);
		if (p != NULL && size > 0)
			addRange(mh, (uintptr_t)p, (uintptr_t)p + size);
	}
	os_unfair_lock_unlock(&lock);
}

static void
removed(const struct mach_header *mh, intptr_t slide)
{
	(void)slide;
	os_unfair_lock_lock(&lock);
	for (size_t i = 0; i < count;)
		if (ranges[i].mh == mh)
			ranges[i] = ranges[--count];
		else
			i++;
	os_unfair_lock_unlock(&lock);
}

bool
_dyld_is_objc_constant(DyldObjCConstantKind kind, const void *addr)
{
	uintptr_t a = (uintptr_t)addr;
	bool found = false;

	if (kind != dyld_objc_string_kind || addr == NULL)
		return (false);
	os_unfair_lock_lock(&lock);
	bool first = !registered;
	registered = true;
	os_unfair_lock_unlock(&lock);
	if (first) {
		/* Called at once for every image already loaded. */
		_dyld_register_func_for_add_image(added);
		_dyld_register_func_for_remove_image(removed);
	}
	os_unfair_lock_lock(&lock);
	for (size_t i = 0; i < count && !found; i++)
		found = a >= ranges[i].start && a < ranges[i].end;
	os_unfair_lock_unlock(&lock);
	return (found);
}
