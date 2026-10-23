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

#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/IOLib.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/ethernet.h>
extern "C" {
#include <net/bpf.h>
}

#define super IONetworkInterface
OSDefineMetaClassAndStructors(IOEthernetInterface, IONetworkInterface);
OSMetaClassDefineReservedUnused(IOEthernetInterface, 0);
OSMetaClassDefineReservedUnused(IOEthernetInterface, 1);
OSMetaClassDefineReservedUnused(IOEthernetInterface, 2);
OSMetaClassDefineReservedUnused(IOEthernetInterface, 3);

static const UInt8 kBroadcastAddress[ETHER_ADDR_LEN] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

bool
IOEthernetInterface::init(IONetworkController *controller)
{
	IOEthernetController *ctl = OSDynamicCast(IOEthernetController, controller);
	IOEthernetAddress addr;

	if (ctl == NULL || !super::init(controller))
		return false;
	if (ctl->getHardwareAddress(&addr) != kIOReturnSuccess) {
		IOLog("%s: no MAC address\n", ctl->getName());
		return false;
	}
	bcopy(addr.bytes, _macAddress, ETHER_ADDR_LEN);

	setInterfaceType(IFT_ETHER);
	setMaxTransferUnit(ETHERMTU);
	setMediaAddressLength(ETHER_ADDR_LEN);
	setMediaHeaderLength(ETHER_HDR_LEN);
	setFlags(IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	return true;
}

const char *
IOEthernetInterface::getNamePrefix() const
{
	return "en";
}

UInt32
IOEthernetInterface::getDataLinkType() const
{
	return DLT_EN10MB;
}

bool
IOEthernetInterface::initIfnetParams(struct ifnet_init_params *params)
{
	if (!super::initIfnetParams(params))
		return false;
	params->uniqueid = _macAddress;
	params->uniqueid_len = ETHER_ADDR_LEN;
	params->family = IFNET_FAMILY_ETHERNET;
	params->demux = ether_demux;
	params->add_proto = ether_add_proto;
	params->del_proto = ether_del_proto;
	params->check_multi = ether_check_multi;
	params->framer = ether_frameout;
	params->broadcast_addr = kBroadcastAddress;
	params->broadcast_len = ETHER_ADDR_LEN;
	return true;
}

const struct sockaddr_dl *
IOEthernetInterface::copyLinkAddress(struct sockaddr_dl *sdl)
{
	bzero(sdl, sizeof(*sdl));
	sdl->sdl_len = sizeof(*sdl);
	sdl->sdl_family = AF_LINK;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	bcopy(_macAddress, LLADDR(sdl), ETHER_ADDR_LEN);
	return sdl;
}

static void
gatedSetMulticastList(void *target, void *ret, void *addrs, void *count, void *)
{
	IOEthernetController *ctl = (IOEthernetController *)target;

	*(IOReturn *)ret = ctl->setMulticastList((IOEthernetAddress *)addrs,
	    *(UInt32 *)count);
}

/* The stack's multicast memberships become the controller's list. */
SInt32
IOEthernetInterface::syncMulticastList()
{
	ifmultiaddr_t *list = NULL;
	IOEthernetAddress *addrs = NULL;
	UInt32 count = 0, n = 0, filters;
	IOReturn ret = kIOReturnSuccess;

	if (ifnet_get_multicast_list(_ifp, &list) != 0)
		return ENOMEM;
	while (list[count] != NULL)
		count++;
	if (count > 0)
		addrs = IONew(IOEthernetAddress, count);
	for (UInt32 i = 0; addrs != NULL && i < count; i++) {
		struct sockaddr_dl sdl;

		if (ifmaddr_lladdress(list[i], (struct sockaddr *)&sdl, sizeof(sdl)) == 0 &&
		    sdl.sdl_family == AF_LINK && sdl.sdl_alen == ETHER_ADDR_LEN)
			bcopy(LLADDR(&sdl), addrs[n++].bytes, ETHER_ADDR_LEN);
	}
	ifnet_free_multicast_list(list);

	_controller->executeCommand(this, gatedSetMulticastList, _controller,
	    &ret, addrs, &n);
	if (addrs != NULL)
		IODelete(addrs, IOEthernetAddress, count);

	filters = _filters & ~kIOPacketFilterMulticast;
	if (n > 0)
		filters |= kIOPacketFilterMulticast;
	/* A controller without a multicast list receives all multicast. */
	if (n > 0 && ret == kIOReturnUnsupported)
		filters |= kIOPacketFilterMulticastAll;
	return setFilters(filters);
}

/* IFF_PROMISC and IFF_ALLMULTI become packet filters. */
SInt32
IOEthernetInterface::syncFlags()
{
	SInt32 err = super::syncFlags();
	UInt16 flags = ifnet_flags(_ifp);
	UInt32 filters = _filters & ~(kIOPacketFilterPromiscuous | kIOPacketFilterMulticastAll);

	if (err != 0)
		return err;
	if (flags & IFF_PROMISC)
		filters |= kIOPacketFilterPromiscuous;
	if (flags & IFF_ALLMULTI)
		filters |= kIOPacketFilterMulticastAll;
	return setFilters(filters);
}

SInt32
IOEthernetInterface::performCommand(IONetworkController *ctl, unsigned long cmd,
    void *arg0, void *data)
{
	SInt32 err;

	switch (cmd) {
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		return syncMulticastList();
	}
	err = super::performCommand(ctl, cmd, arg0, data);
	if (err == EOPNOTSUPP)
		err = ether_ioctl(_ifp, (u_int32_t)cmd, data);
	return err;
}
