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
 * The machine's identity from its firmware's SMBIOS tables, published as
 * a Mac's platform publishes it: the system UUID as /options' platform-uuid
 * (IOPlatformExpert makes it IOPlatformUUID, which gethostuuid() and so
 * CFPreferences' by-host domains wait for), IOPlatformSerialNumber, and
 * the manufacturer, product (the model), version and firmware on the
 * platform device. The loader leaves each EFI configuration table's
 * address in the device tree, the SMBIOS entry point's among them.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IORegistryEntry.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <libkern/crypto/sha1.h>
#include <uuid/uuid.h>

#include "SMBIOS.h"

#define LOG(fmt, ...)	IOLog("SMBIOS: " fmt "\n", ##__VA_ARGS__)

/* The configuration tables' GUIDs, as the loader stores them (EFI order). */
static const uint8_t kSmbiosGuid[16] = {
	0x31, 0x2d, 0x9d, 0xeb, 0x88, 0x2d, 0xd3, 0x11,
	0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d };
static const uint8_t kSmbios3Guid[16] = {
	0x44, 0x15, 0xfd, 0xf2, 0x94, 0x97, 0x2c, 0x4a,
	0x99, 0x2f, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94 };

static char gModel[64];

const char *
MinkarSMBIOSModel(void)
{
	return gModel[0] != '\0' ? gModel : NULL;
}

/* Maps physical memory; NULL when it can't. */
static IOMemoryMap *
map_physical(uint64_t addr, uint64_t len)
{
	IOMemoryDescriptor *d = IOMemoryDescriptor::withPhysicalAddress(
	    (IOPhysicalAddress)addr, (IOByteCount)len, kIODirectionIn);
	IOMemoryMap *m;

	if (d == NULL)
		return NULL;
	m = d->map(kIOMapReadOnly);
	d->release();
	return m;
}

/* The entry point's address: the configuration table of an SMBIOS GUID. */
static uint64_t
entry_point(bool *v3)
{
	IORegistryEntry *tables = IORegistryEntry::fromPath(
	    "/efi/runtime-services/configuration-table", gIODTPlane);
	OSIterator *it;
	OSObject *o;
	uint64_t found = 0;

	if (tables == NULL)
		return 0;
	it = tables->getChildIterator(gIODTPlane);
	while (it != NULL && (o = it->getNextObject()) != NULL) {
		IORegistryEntry *e = OSDynamicCast(IORegistryEntry, o);
		OSData *guid, *table;

		if (e == NULL)
			continue;
		guid = OSDynamicCast(OSData, e->getProperty("guid"));
		table = OSDynamicCast(OSData, e->getProperty("table"));
		if (guid == NULL || table == NULL || guid->getLength() != 16 ||
		    table->getLength() != sizeof(uint64_t))
			continue;
		/* SMBIOS 3 (64-bit) is preferred to the 32-bit entry point. */
		if (memcmp(guid->getBytesNoCopy(), kSmbios3Guid, 16) == 0) {
			found = *(const uint64_t *)table->getBytesNoCopy();
			*v3 = true;
			break;
		}
		if (memcmp(guid->getBytesNoCopy(), kSmbiosGuid, 16) == 0) {
			found = *(const uint64_t *)table->getBytesNoCopy();
			*v3 = false;
		}
	}
	if (it != NULL)
		it->release();
	tables->release();
	return found;
}

/* A structure's string n (1-based), or NULL; placeholders count as none. */
static const char *
smbios_string(const uint8_t *s, const uint8_t *end, uint8_t n)
{
	const char *p = (const char *)s + s[1];
	static const char *const placeholders[] = {
		"Default string", "To be filled by O.E.M.", "To Be Filled By O.E.M.",
		"System Serial Number", "System Product Name", "Not Specified",
		"None", "0123456789", "Unknown", "",
	};

	if (n == 0)
		return NULL;
	while (--n > 0 && (const uint8_t *)p < end) {
		p += strnlen(p, (size_t)(end - (const uint8_t *)p)) + 1;
		if ((const uint8_t *)p >= end || *p == '\0')
			return NULL;
	}
	if ((const uint8_t *)p >= end)
		return NULL;
	for (size_t i = 0; i < sizeof(placeholders) / sizeof(placeholders[0]); i++)
		if (strcmp(p, placeholders[i]) == 0)
			return NULL;
	return p;
}

static void
set_data(IOService *dev, const char *key, const char *value)
{
	OSData *d;

	if (value == NULL)
		return;
	d = OSData::withBytes(value, (unsigned int)strlen(value) + 1);
	if (d != NULL) {
		dev->setProperty(key, d);
		d->release();
	}
}

void
MinkarSMBIOSPublish(IOService *platformDevice)
{
	bool v3 = false;
	uint64_t ep = entry_point(&v3), table, size;
	IOMemoryMap *epmap, *tmap;
	const uint8_t *e, *s, *end;
	const char *serial = NULL, *board_serial = NULL;
	bool uuid_done = false;

	if (ep == 0) {
		LOG("no SMBIOS table from the firmware");
		return;
	}
	if ((epmap = map_physical(ep, 32)) == NULL)
		return;
	e = (const uint8_t *)epmap->getVirtualAddress();
	if (v3 && memcmp(e, "_SM3_", 5) == 0) {
		uint32_t sz32;

		memcpy(&sz32, e + 0x0c, sizeof(sz32));
		memcpy(&table, e + 0x10, sizeof(table));
		size = sz32;
	} else if (!v3 && memcmp(e, "_SM_", 4) == 0) {
		uint16_t sz16;
		uint32_t t32;

		memcpy(&sz16, e + 0x16, sizeof(sz16));
		memcpy(&t32, e + 0x18, sizeof(t32));
		size = sz16;
		table = t32;
	} else {
		LOG("the entry point is not valid");
		epmap->release();
		return;
	}
	epmap->release();
	if (size == 0 || size > 1024 * 1024 || (tmap = map_physical(table, size)) ==
	    NULL)
		return;
	s = (const uint8_t *)tmap->getVirtualAddress();
	end = s + size;

	/* Each structure: type, length, handle, formatted area, strings, "\0\0". */
	while (s + 4 <= end && s[1] >= 4 && s + s[1] <= end) {
		const uint8_t *next = s + s[1];
		uint8_t type = s[0];

		while (next + 1 < end && (next[0] != 0 || next[1] != 0))
			next++;
		next += 2;
		if (type == 127)		/* end of table */
			break;
		if (type == 0 && s[1] >= 0x09) {	/* firmware */
			set_data(platformDevice, "bios-vendor",
			    smbios_string(s, end, s[0x04]));
			set_data(platformDevice, "bios-version",
			    smbios_string(s, end, s[0x05]));
			set_data(platformDevice, "bios-release-date",
			    smbios_string(s, end, s[0x08]));
		} else if (type == 1 && s[1] >= 0x08) {	/* system */
			const char *product = smbios_string(s, end, s[0x05]);

			set_data(platformDevice, "manufacturer",
			    smbios_string(s, end, s[0x04]));
			set_data(platformDevice, "product-name", product);
			set_data(platformDevice, "model", product);
			set_data(platformDevice, "version",
			    smbios_string(s, end, s[0x06]));
			serial = smbios_string(s, end, s[0x07]);
			if (product != NULL)
				strlcpy(gModel, product, sizeof(gModel));
			if (s[1] >= 0x19 && !uuid_done) {
				const uint8_t *u = s + 0x08;
				uint8_t rfc[16];
				bool zero = true, ones = true;

				for (int i = 0; i < 16; i++) {
					zero = zero && u[i] == 0x00;
					ones = ones && u[i] == 0xff;
				}
				if (!zero && !ones) {
					/* The first three fields are little-endian. */
					rfc[0] = u[3]; rfc[1] = u[2]; rfc[2] = u[1];
					rfc[3] = u[0]; rfc[4] = u[5]; rfc[5] = u[4];
					rfc[6] = u[7]; rfc[7] = u[6];
					memcpy(rfc + 8, u + 8, 8);
					IORegistryEntry *options = IORegistryEntry::fromPath(
					    "/options", gIODTPlane);

					if (options != NULL) {
						OSData *d = OSData::withBytes(rfc, 16);

						if (d != NULL) {
							options->setProperty("platform-uuid", d);
							d->release();
						}
						options->release();
						uuid_done = true;
					}
				}
			}
		} else if (type == 2 && s[1] >= 0x08) {	/* baseboard */
			set_data(platformDevice, "board-id",
			    smbios_string(s, end, s[0x05]));
			board_serial = smbios_string(s, end, s[0x07]);
		}
		if (next > end)
			break;
		s = next;
	}

	/*
	 * No UUID from the firmware (a virtual machine's may be empty): one
	 * derived from the boot volume's, stable for this installation.
	 */
	if (!uuid_done) {
		IORegistryEntry *chosen = IORegistryEntry::fromPath("/chosen",
		    gIODTPlane);
		IORegistryEntry *options = IORegistryEntry::fromPath("/options",
		    gIODTPlane);
		OSData *boot = chosen ? OSDynamicCast(OSData,
		    chosen->getProperty("boot-uuid")) : NULL;

		if (boot != NULL && boot->getLength() > 1 && options != NULL) {
			static const uint8_t space[16] = {
				0x6d, 0x69, 0x6e, 0x6b, 0x61, 0x72, 0x40, 0x70,
				0x80, 0x6c, 0x61, 0x74, 0x66, 0x6f, 0x72, 0x6d };
			uint8_t digest[SHA1_RESULTLEN];
			SHA1_CTX ctx;
			OSData *d;

			SHA1Init(&ctx);
			SHA1Update(&ctx, space, sizeof(space));
			SHA1Update(&ctx, boot->getBytesNoCopy(), boot->getLength());
			SHA1Final(digest, &ctx);
			digest[6] = (digest[6] & 0x0f) | 0x50;	/* version 5 */
			digest[8] = (digest[8] & 0x3f) | 0x80;	/* RFC 4122 */
			if ((d = OSData::withBytes(digest, 16)) != NULL) {
				options->setProperty("platform-uuid", d);
				d->release();
				uuid_done = true;
			}
		}
		if (chosen != NULL)
			chosen->release();
		if (options != NULL)
			options->release();
	}

	/*
	 * Publish it as IOPlatformExpert does when an NVRAM controller
	 * registers (ours does not): gethostuuid() waits for this resource.
	 */
	if (uuid_done) {
		IORegistryEntry *options = IORegistryEntry::fromPath("/options",
		    gIODTPlane);
		OSData *d = options ? OSDynamicCast(OSData,
		    options->getProperty("platform-uuid")) : NULL;

		if (d != NULL && d->getLength() == sizeof(uuid_t)) {
			uuid_string_t str;
			OSString *u;

			uuid_unparse_upper((const uint8_t *)d->getBytesNoCopy(), str);
			if ((u = OSString::withCString(str)) != NULL) {
				platformDevice->setProperty(kIOPlatformUUIDKey, u);
				IOService::publishResource(kIOPlatformUUIDKey, u);
				u->release();
			}
		}
		if (options != NULL)
			options->release();
	}

	if (serial == NULL)
		serial = board_serial;
	if (serial != NULL) {
		OSString *str = OSString::withCString(serial);

		if (str != NULL) {
			platformDevice->setProperty(kIOPlatformSerialNumberKey, str);
			str->release();
		}
		set_data(platformDevice, "serial-number", serial);
	}
	LOG("%s%s%s%s", gModel[0] ? gModel : "(no product name)",
	    serial ? ", serial " : "", serial ? serial : "",
	    uuid_done ? ", platform UUID" : ", no platform UUID");
	tmap->release();
}
