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
 * IONetworkUserClient: what IOKit's IONetworkLib (IONetworkOpen,
 * IONetworkReadData, IONetworkSetPacketFiltersMask, ...) talks to on a
 * network interface. Reading data is open to everyone; writing, resetting
 * and changing properties need administrator rights.
 */

#ifndef _IONETWORKUSERCLIENT_H
#define _IONETWORKUSERCLIENT_H

#include <IOKit/IOUserClient.h>

#define kIONetworkUserClientTypeID	0xff000001
#define kIONUCType			0xff000001

/* Selectors. */
enum {
	kIONUCResetNetworkDataIndex		= 0,
	kIONUCWriteNetworkDataIndex		= 1,
	kIONUCReadNetworkDataIndex		= 2,
	kIONUCGetNetworkDataCapacityIndex	= 3,
	kIONUCGetNetworkDataHandleIndex		= 4,
	kIONUCLastIndex
};

#ifdef KERNEL

class IONetworkInterface;

class IONetworkUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IONetworkUserClient);

protected:
	IONetworkInterface *	_owner;
	task_t			_task;
	bool			_privileged;

public:
	static IONetworkUserClient *withTask(task_t owningTask, void *securityID);

	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientDied() APPLE_KEXT_OVERRIDE;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
	/* The interface, for IOConnectGetService(). */
	virtual IOService *getService() APPLE_KEXT_OVERRIDE;
};

#endif /* KERNEL */

#endif /* !_IONETWORKUSERCLIENT_H */
