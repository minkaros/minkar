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
 * IOEthernetInterface: an Ethernet network interface (en0, ...). It uses
 * the stack's Ethernet framing (ether_frameout, ether_demux), tracks the
 * multicast addresses the stack joins and turns IFF_PROMISC and
 * IFF_ALLMULTI into the controller's packet filters.
 */

#ifndef _IOETHERNETINTERFACE_H
#define _IOETHERNETINTERFACE_H

#include <IOKit/network/IONetworkInterface.h>

#define kIOEthernetInterfaceClass	"IOEthernetInterface"

class IOEthernetController;

class IOEthernetInterface : public IONetworkInterface
{
	OSDeclareDefaultStructors(IOEthernetInterface);

protected:
	UInt8		_macAddress[6];

	virtual bool initIfnetParams(struct ifnet_init_params *params) APPLE_KEXT_OVERRIDE;
	virtual const struct sockaddr_dl *copyLinkAddress(struct sockaddr_dl *buf) APPLE_KEXT_OVERRIDE;
	virtual SInt32 performCommand(IONetworkController *controller,
	    unsigned long cmd, void *arg0, void *arg1) APPLE_KEXT_OVERRIDE;
	virtual SInt32 syncFlags() APPLE_KEXT_OVERRIDE;
	virtual UInt32 getDataLinkType() const APPLE_KEXT_OVERRIDE;

	/* Gives the controller the stack's list of multicast addresses. */
	virtual SInt32 syncMulticastList();

public:
	virtual bool init(IONetworkController *controller) APPLE_KEXT_OVERRIDE;
	virtual const char *getNamePrefix() const APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IOEthernetInterface, 0);
	OSMetaClassDeclareReservedUnused(IOEthernetInterface, 1);
	OSMetaClassDeclareReservedUnused(IOEthernetInterface, 2);
	OSMetaClassDeclareReservedUnused(IOEthernetInterface, 3);
};

#endif /* !_IOETHERNETINTERFACE_H */
