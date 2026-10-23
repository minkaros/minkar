/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * IONetworkStack: names network interfaces. A published interface waits
 * here, without a BSD interface, until user space names it: configd's
 * InterfaceNamer opens this service and sets the property
 * kIONetworkStackUserCommandKey with the interface (IORegistryEntryID or
 * IOPathMatch) and the unit it keeps for it across boots. When the
 * personality's IONetworkStackAutoRegister is true, or no namer appears
 * within IONetworkStackNamerTimeout seconds, interfaces get the lowest free
 * unit in the order they appear.
 */

#ifndef _IONETWORKSTACK_H
#define _IONETWORKSTACK_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

#define kIONetworkStackUserCommandKey	"IONetworkStackUserCommand"
#define kIONetworkStackUserCommand	kIONetworkStackUserCommandKey

/* User commands. */
enum {
	kIONetworkStackRegisterInterfaceWithUnit	= 0,
	kIONetworkStackRegisterInterfaceWithLowestUnit	= 1,
	kIONetworkStackRegisterInterfaceAll		= 2,
};

class IONetworkInterface;

class IONetworkStack : public IOService
{
	OSDeclareDefaultStructors(IONetworkStack);

protected:
	IOLock *		_lock;
	OSSet *			_interfaces;	/* published, not yet named */
	bool			_autoRegister;
	bool			_namerSeen;
	thread_call_t		_namerTimer;

	virtual void free() APPLE_KEXT_OVERRIDE;
	static void namerTimeout(thread_call_param_t stack, thread_call_param_t);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID,
	    UInt32 type, IOUserClient **handler) APPLE_KEXT_OVERRIDE;

	/* Called by IONetworkInterface::registerService(). */
	static void interfacePublished(IONetworkInterface *netif);

	/* Names an interface: unit exactly, or the lowest free unit >= unit. */
	IOReturn registerInterface(IONetworkInterface *netif, UInt32 unit, bool fixedUnit);
	IOReturn registerAllInterfaces();
	IONetworkInterface *findInterface(OSDictionary *command);
	void namerConnected();

	/* Units in use per name prefix (en, ...). */
	static bool reserveUnit(const char *prefix, UInt32 unit);
	static bool lowestFreeUnit(const char *prefix, UInt32 from, UInt32 *unit);
	static void releaseUnit(const char *prefix, UInt32 unit);
};

class IONetworkStackUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IONetworkStackUserClient);

protected:
	IONetworkStack *	_stack;

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
	virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
	virtual IOService *getService() APPLE_KEXT_OVERRIDE;
};

#endif /* !_IONETWORKSTACK_H */
