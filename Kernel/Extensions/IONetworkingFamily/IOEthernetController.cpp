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

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSData.h>

const OSSymbol *gIOEthernetWakeOnLANFilterGroup;

#define super IONetworkController
OSDefineMetaClassAndAbstractStructors(IOEthernetController, IONetworkController);
OSMetaClassDefineReservedUnused(IOEthernetController, 0);
OSMetaClassDefineReservedUnused(IOEthernetController, 1);
OSMetaClassDefineReservedUnused(IOEthernetController, 2);
OSMetaClassDefineReservedUnused(IOEthernetController, 3);
OSMetaClassDefineReservedUnused(IOEthernetController, 4);
OSMetaClassDefineReservedUnused(IOEthernetController, 5);
OSMetaClassDefineReservedUnused(IOEthernetController, 6);
OSMetaClassDefineReservedUnused(IOEthernetController, 7);

void
IOEthernetController::initialize()
{
	if (gIOEthernetWakeOnLANFilterGroup == NULL) {
		const OSSymbol *sym =
		    OSSymbol::withCStringNoCopy(kIOEthernetWakeOnLANFilterGroup);

		if (sym != NULL && !OSCompareAndSwapPtr(NULL, __DECONST(void *, sym),
		    (void * volatile *)&gIOEthernetWakeOnLANFilterGroup))
			sym->release();
	}
}

bool
IOEthernetController::init(OSDictionary *properties)
{
	initialize();
	return super::init(properties) && gIOEthernetWakeOnLANFilterGroup != NULL;
}

IONetworkInterface *
IOEthernetController::createInterface()
{
	IOEthernetInterface *netif = new IOEthernetInterface;

	if (netif != NULL && !netif->init(this)) {
		netif->release();
		netif = NULL;
	}
	return netif;
}

/* kIOMACAddress, as on macOS. */
bool
IOEthernetController::publishProperties()
{
	IOEthernetAddress addr;

	if (getHardwareAddress(&addr) == kIOReturnSuccess)
		setProperty(kIOMACAddress, (void *)addr.bytes, kIOEthernetAddressSize);
	return super::publishProperties();
}

IOReturn
IOEthernetController::setHardwareAddress(const IOEthernetAddress *)
{
	return kIOReturnUnsupported;
}

IOReturn IOEthernetController::setMulticastMode(bool) { return kIOReturnUnsupported; }
IOReturn IOEthernetController::setMulticastList(IOEthernetAddress *, UInt32) { return kIOReturnUnsupported; }
IOReturn IOEthernetController::setPromiscuousMode(bool) { return kIOReturnUnsupported; }
IOReturn IOEthernetController::setWakeOnMagicPacket(bool) { return kIOReturnUnsupported; }

IOReturn
IOEthernetController::getHardwareAddress(void *addr, UInt32 *inOutAddrBytes)
{
	IOEthernetAddress ea;
	IOReturn ret;

	if (addr == NULL || inOutAddrBytes == NULL)
		return kIOReturnBadArgument;
	if (*inOutAddrBytes < kIOEthernetAddressSize) {
		*inOutAddrBytes = kIOEthernetAddressSize;
		return kIOReturnBadArgument;
	}
	if ((ret = getHardwareAddress(&ea)) == kIOReturnSuccess) {
		bcopy(ea.bytes, addr, kIOEthernetAddressSize);
		*inOutAddrBytes = kIOEthernetAddressSize;
	}
	return ret;
}

IOReturn
IOEthernetController::setHardwareAddress(const void *addr, UInt32 addrBytes)
{
	if (addr == NULL || addrBytes != kIOEthernetAddressSize)
		return kIOReturnBadArgument;
	return setHardwareAddress((const IOEthernetAddress *)addr);
}

IOReturn
IOEthernetController::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
	if (filters == NULL)
		return kIOReturnBadArgument;
	if (group == gIONetworkFilterGroup) {
		*filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast |
		    kIOPacketFilterMulticast | kIOPacketFilterMulticastAll |
		    kIOPacketFilterPromiscuous;
		return kIOReturnSuccess;
	}
	if (group == gIOEthernetWakeOnLANFilterGroup) {
		*filters = 0;
		return kIOReturnSuccess;
	}
	return super::getPacketFilters(group, filters);
}

/*
 * The generic filters become the Ethernet calls. Unicast and broadcast
 * are always on; the multicast list itself comes through setMulticastList().
 */
IOReturn
IOEthernetController::enablePacketFilter(const OSSymbol *group, UInt32 aFilter,
    UInt32, IOOptionBits)
{
	if (group == gIOEthernetWakeOnLANFilterGroup)
		return aFilter == kIOEthernetWakeOnMagicPacket ?
		    setWakeOnMagicPacket(true) : kIOReturnUnsupported;
	if (group != gIONetworkFilterGroup)
		return kIOReturnUnsupported;
	switch (aFilter) {
	case kIOPacketFilterUnicast:
	case kIOPacketFilterBroadcast:
	case kIOPacketFilterMulticast:
		return kIOReturnSuccess;
	case kIOPacketFilterMulticastAll:
		return setMulticastMode(true);
	case kIOPacketFilterPromiscuous:
		return setPromiscuousMode(true);
	default:
		return kIOReturnUnsupported;
	}
}

IOReturn
IOEthernetController::disablePacketFilter(const OSSymbol *group, UInt32 aFilter,
    UInt32, IOOptionBits)
{
	if (group == gIOEthernetWakeOnLANFilterGroup)
		return aFilter == kIOEthernetWakeOnMagicPacket ?
		    setWakeOnMagicPacket(false) : kIOReturnUnsupported;
	if (group != gIONetworkFilterGroup)
		return kIOReturnUnsupported;
	switch (aFilter) {
	case kIOPacketFilterUnicast:
	case kIOPacketFilterBroadcast:
		return kIOReturnUnsupported;	/* always on */
	case kIOPacketFilterMulticast:
		setMulticastList(NULL, 0);
		return kIOReturnSuccess;
	case kIOPacketFilterMulticastAll:
		return setMulticastMode(false);
	case kIOPacketFilterPromiscuous:
		return setPromiscuousMode(false);
	default:
		return kIOReturnUnsupported;
	}
}

IOReturn
IOEthernetController::getMaxPacketSize(UInt32 *maxSize) const
{
	if (maxSize == NULL)
		return kIOReturnBadArgument;
	*maxSize = kIOEthernetMaxPacketSize;
	return kIOReturnSuccess;
}

IOReturn
IOEthernetController::getMinPacketSize(UInt32 *minSize) const
{
	if (minSize == NULL)
		return kIOReturnBadArgument;
	*minSize = kIOEthernetMinPacketSize;
	return kIOReturnSuccess;
}
