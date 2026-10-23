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
 * IOEthernetController: the base class of Ethernet drivers. Its interface
 * is an IOEthernetInterface (en0, ...); the generic packet filter calls of
 * IONetworkController become setPromiscuousMode(), setMulticastMode() and
 * setMulticastList().
 */

#ifndef _IOETHERNETCONTROLLER_H
#define _IOETHERNETCONTROLLER_H

#include <IOKit/network/IONetworkController.h>

#define kIOEthernetControllerClass	"IOEthernetController"
#define kIOEthernetAddressSize		6

/* Frame sizes, CRC included. */
enum {
	kIOEthernetCRCSize		= 4,
	kIOEthernetMinPacketSize	= 64,
	kIOEthernetMaxPacketSize	= 1518,
};

typedef struct {
	UInt8 bytes[kIOEthernetAddressSize];
} IOEthernetAddress;

/* The Wake-on-LAN filter group and its filters. */
#define kIOEthernetWakeOnLANFilterGroup	"IOEthernetWakeOnLANFilterGroup"
#ifdef KERNEL
extern const OSSymbol *gIOEthernetWakeOnLANFilterGroup;
#endif /* KERNEL */
enum {
	kIOEthernetWakeOnMagicPacket	= 0x00000001,
};

#ifdef KERNEL
class IOEthernetInterface;
#endif /* KERNEL */

#ifdef KERNEL
class IOEthernetController : public IONetworkController
{
	OSDeclareAbstractStructors(IOEthernetController);

protected:
	virtual IONetworkInterface *createInterface() APPLE_KEXT_OVERRIDE;
	virtual bool publishProperties() APPLE_KEXT_OVERRIDE;

public:
	static void initialize();

	virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;

	virtual IOReturn getHardwareAddress(IOEthernetAddress *addr) = 0;
	virtual IOReturn setHardwareAddress(const IOEthernetAddress *addr);
	virtual IOReturn setMulticastMode(bool active);
	virtual IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count);
	virtual IOReturn setPromiscuousMode(bool active);
	virtual IOReturn setWakeOnMagicPacket(bool active);

	virtual IOReturn getHardwareAddress(void *addr,
	    UInt32 *inOutAddrBytes) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setHardwareAddress(const void *addr,
	    UInt32 addrBytes) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getPacketFilters(const OSSymbol *group,
	    UInt32 *filters) const APPLE_KEXT_OVERRIDE;
	virtual IOReturn enablePacketFilter(const OSSymbol *group, UInt32 aFilter,
	    UInt32 enabledFilters, IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn disablePacketFilter(const OSSymbol *group, UInt32 aFilter,
	    UInt32 enabledFilters, IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getMaxPacketSize(UInt32 *maxSize) const APPLE_KEXT_OVERRIDE;
	virtual IOReturn getMinPacketSize(UInt32 *minSize) const APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IOEthernetController, 0);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 1);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 2);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 3);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 4);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 5);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 6);
	OSMetaClassDeclareReservedUnused(IOEthernetController, 7);
};
#endif /* KERNEL */

/*
 * A driver includes this header to subclass IOEthernetController; the
 * interface class comes with it, as on macOS.
 */
#include <IOKit/network/IOEthernetInterface.h>

#endif /* !_IOETHERNETCONTROLLER_H */
