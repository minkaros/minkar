/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HFS+ compressed files: registers the compression types of
 * MinkarFSCompressionTypes.c with the kernel's decmpfs when I/O Kit starts
 * it (once BSD is up, decmpfs with it: see Info.plist), and publishes the
 * resource each type's waiters look for. A compressed file read before
 * then waits for us: decmpfs finds the types in our personality.
 */

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

#include "MinkarFSCompressionTypes.h"

class MinkarFSCompression : public IOService {
	OSDeclareDefaultStructors(MinkarFSCompression);
	UInt32	fRegistered;	/* a bit per entry of MinkarFSCompressionTypes */
public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(MinkarFSCompression, IOService);

bool
MinkarFSCompression::start(IOService *provider)
{
	char name[64];

	if (!IOService::start(provider))
		return false;
	for (unsigned i = 0; i < MinkarFSCompressionTypeCount; i++) {
		int err = MinkarFSCompressionRegister(MinkarFSCompressionTypes[i]);

		if (err != 0) {
			IOLog("MinkarFSCompression: type %u not registered (%d)\n",
			    MinkarFSCompressionTypes[i], err);
			continue;
		}
		fRegistered |= 1u << i;
		/* What decmpfs waits for, having found the type in our plist. */
		snprintf(name, sizeof(name), "com.apple.AppleFSCompression.Type%u",
		    MinkarFSCompressionTypes[i]);
		publishResource(name);
	}
	IOLog("MinkarFSCompression: zlib, LZVN and LZFSE compressed files readable\n");
	registerService();
	return true;
}

void
MinkarFSCompression::stop(IOService *provider)
{
	for (unsigned i = 0; i < MinkarFSCompressionTypeCount; i++)
		if (fRegistered & (1u << i))
			MinkarFSCompressionUnregister(MinkarFSCompressionTypes[i]);
	fRegistered = 0;
	IOService::stop(provider);
}
