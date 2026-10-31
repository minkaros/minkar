/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The two property dictionaries a block storage device publishes on macOS,
 * "Device Characteristics" (vendor, product, revision, medium) and
 * "Protocol Characteristics" (interconnect and where it is), for Minkar's
 * storage drivers. Not installed: a helper for the drivers only.
 */

#ifndef _MINKAR_STORAGE_CHARACTERISTICS_H_
#define _MINKAR_STORAGE_CHARACTERISTICS_H_

#include <IOKit/IOService.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

static inline void
MinkarSetStringIfAny(OSDictionary *d, const char *key, const char *value)
{
	OSString *s;
	char buf[64];
	size_t n;

	if (value == NULL)
		return;
	/* Drives pad their identify strings with spaces. */
	strlcpy(buf, value, sizeof(buf));
	for (n = strlen(buf); n > 0 && buf[n - 1] == ' '; n--)
		buf[n - 1] = '\0';
	if (buf[0] == '\0')
		return;
	s = OSString::withCString(buf);
	if (s != NULL) {
		d->setObject(key, s);
		s->release();
	}
}

/* medium may be NULL when the device does not say. */
static inline void
MinkarSetStorageCharacteristics(IOService *device, const char *vendor,
    const char *product, const char *revision, const char *medium,
    const char *interconnect, bool internal)
{
	OSDictionary *dc = OSDictionary::withCapacity(4);
	OSDictionary *pc = OSDictionary::withCapacity(2);

	if (dc != NULL) {
		MinkarSetStringIfAny(dc, kIOPropertyVendorNameKey, vendor);
		MinkarSetStringIfAny(dc, kIOPropertyProductNameKey, product);
		MinkarSetStringIfAny(dc, kIOPropertyProductRevisionLevelKey,
		    revision);
		MinkarSetStringIfAny(dc, kIOPropertyMediumTypeKey, medium);
		device->setProperty(kIOPropertyDeviceCharacteristicsKey, dc);
		dc->release();
	}
	if (pc != NULL) {
		MinkarSetStringIfAny(pc, kIOPropertyPhysicalInterconnectTypeKey,
		    interconnect);
		MinkarSetStringIfAny(pc, kIOPropertyPhysicalInterconnectLocationKey,
		    internal ? kIOPropertyInternalKey : kIOPropertyExternalKey);
		device->setProperty(kIOPropertyProtocolCharacteristicsKey, pc);
		pc->release();
	}
}

#endif /* _MINKAR_STORAGE_CHARACTERISTICS_H_ */
