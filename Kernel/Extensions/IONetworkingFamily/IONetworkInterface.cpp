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

#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <libkern/OSAtomic.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/kern_event.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/net_kev.h>
extern "C" {
#include <net/bpf.h>
}

#define super IOService
OSDefineMetaClassAndAbstractStructors(IONetworkInterface, IOService);
OSMetaClassDefineReservedUnused(IONetworkInterface, 0);
OSMetaClassDefineReservedUnused(IONetworkInterface, 1);
OSMetaClassDefineReservedUnused(IONetworkInterface, 2);
OSMetaClassDefineReservedUnused(IONetworkInterface, 3);
OSMetaClassDefineReservedUnused(IONetworkInterface, 4);
OSMetaClassDefineReservedUnused(IONetworkInterface, 5);
OSMetaClassDefineReservedUnused(IONetworkInterface, 6);
OSMetaClassDefineReservedUnused(IONetworkInterface, 7);

/*
 * Unit numbers, per name prefix, in the order interfaces register: the
 * first Ethernet interface is en0. (macOS keeps them across boots in the
 * IONetworkStack; that comes later.)
 */
#define MAX_PREFIXES	8
static struct {
	char	prefix[IFNAMSIZ];
	UInt32	next;
} sUnits[MAX_PREFIXES];
static IOLock *sUnitLock;

static UInt32
allocateUnit(const char *prefix)
{
	UInt32 unit = 0;
	int i;

	if (sUnitLock == NULL) {
		IOLock *lock = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&sUnitLock))
			IOLockFree(lock);
	}
	IOLockLock(sUnitLock);
	for (i = 0; i < MAX_PREFIXES; i++) {
		if (sUnits[i].prefix[0] == '\0')
			strlcpy(sUnits[i].prefix, prefix, sizeof(sUnits[i].prefix));
		if (strncmp(sUnits[i].prefix, prefix, sizeof(sUnits[i].prefix)) == 0) {
			unit = sUnits[i].next++;
			break;
		}
	}
	IOLockUnlock(sUnitLock);
	return unit;
}

bool
IONetworkInterface::init(IONetworkController *controller)
{
	if (controller == NULL || !super::init())
		return false;
	_controller = controller;
	_lock = IOLockAlloc();
	_mtu = 1500;
	_filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast;
	setProperty(kIOInterfaceState, (unsigned long long)0, 32);
	return _lock != NULL;
}

void
IONetworkInterface::free()
{
	clearInputQueue();
	if (_ifp != NULL) {
		ifnet_release(_ifp);
		_ifp = NULL;
	}
	if (_lock != NULL) {
		IOLockFree(_lock);
		_lock = NULL;
	}
	super::free();
}

IONetworkController *IONetworkInterface::getController() const { return _controller; }
ifnet_t IONetworkInterface::getIfnet() const { return _ifp; }
UInt32 IONetworkInterface::getUnitNumber() const { return _unit; }
bool IONetworkInterface::isPrimaryInterface() const { return _attached && _unit == 0; }
UInt8 IONetworkInterface::getInterfaceType() const { return _type; }
UInt32 IONetworkInterface::getMaxTransferUnit() const { return _mtu; }
UInt32 IONetworkInterface::getMediaAddressLength() const { return _addressLength; }
UInt32 IONetworkInterface::getMediaHeaderLength() const { return _headerLength; }

bool
IONetworkInterface::setInterfaceType(UInt8 type)
{
	_type = type;
	setProperty(kIOInterfaceType, type, 8);
	return true;
}

bool
IONetworkInterface::setMaxTransferUnit(UInt32 mtu)
{
	_mtu = mtu;
	if (_ifp != NULL)
		ifnet_set_mtu(_ifp, mtu);
	setProperty(kIOMaxTransferUnit, mtu, 32);
	return true;
}

bool
IONetworkInterface::setMediaAddressLength(UInt32 length)
{
	_addressLength = (UInt8)length;
	setProperty(kIOMediaAddressLength, length, 32);
	return true;
}

bool
IONetworkInterface::setMediaHeaderLength(UInt32 length)
{
	_headerLength = (UInt8)length;
	if (_ifp != NULL)
		ifnet_set_hdrlen(_ifp, length);
	setProperty(kIOMediaHeaderLength, length, 32);
	return true;
}

UInt32
IONetworkInterface::getFlags() const
{
	return _ifp != NULL ? (UInt32)ifnet_flags(_ifp) : _flags;
}

bool
IONetworkInterface::setFlags(UInt32 flags, UInt32 clear)
{
	_flags = (_flags & ~clear) | flags;
	if (_ifp != NULL)
		ifnet_set_flags(_ifp, (UInt16)flags, (UInt16)(flags | clear));
	setProperty(kIOInterfaceFlags, getFlags(), 32);
	return true;
}

/* Generic parameters; IOEthernetInterface adds the Ethernet functions. */
bool
IONetworkInterface::initIfnetParams(struct ifnet_init_params *params)
{
	params->name = getNamePrefix();
	params->unit = _unit;
	params->type = _type;
	params->output = ifOutput;
	params->ioctl = ifIoctl;
	params->detach = ifDetached;
	params->softc = this;
	return true;
}

const struct sockaddr_dl *
IONetworkInterface::copyLinkAddress(struct sockaddr_dl *)
{
	return NULL;
}

UInt32
IONetworkInterface::getDataLinkType() const
{
	return DLT_NULL;
}

/*
 * Creates and attaches the ifnet. The interface stays retained while the
 * ifnet exists; ifDetached() drops that reference.
 */
bool
IONetworkInterface::attachToDataLinkLayer(IOOptionBits, void *)
{
	struct ifnet_init_params params;
	struct sockaddr_dl sdl;
	const struct sockaddr_dl *lladdr;
	char bsdName[IFNAMSIZ];
	errno_t err;

	if (_attached)
		return true;
	_unit = allocateUnit(getNamePrefix());

	bzero(&params, sizeof(params));
	if (!initIfnetParams(&params))
		return false;
	if ((err = ifnet_allocate(&params, &_ifp)) != 0) {
		IOLog("%s: ifnet_allocate failed (%d)\n", getName(), err);
		_ifp = NULL;
		return false;
	}
	ifnet_set_mtu(_ifp, _mtu);
	ifnet_set_hdrlen(_ifp, _headerLength);
	ifnet_set_addrlen(_ifp, _addressLength);
	if (_flags != 0)
		ifnet_set_flags(_ifp, (UInt16)_flags, (UInt16)_flags);

	lladdr = copyLinkAddress(&sdl);
	retain();
	if ((err = ifnet_attach(_ifp, lladdr)) != 0) {
		IOLog("%s: ifnet_attach failed (%d)\n", getName(), err);
		ifnet_release(_ifp);
		_ifp = NULL;
		release();
		return false;
	}
	bpfattach(_ifp, getDataLinkType(), _headerLength);
	_attached = true;

	snprintf(bsdName, sizeof(bsdName), "%s%u", getNamePrefix(), _unit);
	setProperty(kIOBSDNameKey, bsdName);
	setProperty(kIOInterfaceUnit, _unit, 32);
	setProperty(kIOInterfaceNamePrefix, getNamePrefix());
	setProperty(kIOPrimaryInterface, _unit == 0);
	setProperty(kIOBuiltin, true);
	setProperty(kIOInterfaceState, kIONetworkInterfaceRegisteredState, 32);
	IOLog("%s: attached as %s\n", _controller->getName(), bsdName);
	return true;
}

void
IONetworkInterface::detachFromDataLinkLayer(IOOptionBits, void *)
{
	if (!_attached)
		return;
	_attached = false;
	ifnet_detach(_ifp);	/* ifDetached() runs when the stack lets go */
}

void
IONetworkInterface::ifDetached(ifnet_t ifp)
{
	IONetworkInterface *netif = (IONetworkInterface *)ifnet_softc(ifp);

	if (netif != NULL)
		netif->release();
}

void
IONetworkInterface::registerService(IOOptionBits options)
{
	if (!attachToDataLinkLayer(0, NULL))
		IOLog("%s: no BSD interface\n", getName());
	super::registerService(options);
}

/* Packets from the stack, already framed. */
errno_t
IONetworkInterface::ifOutput(ifnet_t ifp, mbuf_t m)
{
	IONetworkInterface *netif = (IONetworkInterface *)ifnet_softc(ifp);
	UInt32 status;

	if (netif == NULL || !netif->_enabled) {
		mbuf_freem_list(m);
		return ENETDOWN;
	}
	for (mbuf_t n = m; n != NULL; n = mbuf_nextpkt(n)) {
		bpf_tap_out(ifp, netif->getDataLinkType(), n, NULL, 0);
		ifnet_stat_increment_out(ifp, 1, (UInt32)mbuf_pkthdr_len(n), 0);
	}
	if (netif->_outputQueue != NULL) {
		netif->_outputQueue->enqueue(m, NULL);
		return 0;
	}
	/* No queue: a controller without one must not stall. */
	for (mbuf_t next; m != NULL; m = next) {
		next = mbuf_nextpkt(m);
		mbuf_setnextpkt(m, NULL);
		status = netif->_controller->outputPacket(m, NULL);
		if ((status & kIOOutputStatusMask) == kIOReturnOutputStall)
			mbuf_freem(m);
	}
	return 0;
}

UInt32
IONetworkInterface::inputPacket(mbuf_t m, UInt32 length, IOOptionBits options, void *)
{
	UInt32 len;

	if (m == NULL)
		return 0;
	if (!_attached || !_enabled) {
		mbuf_freem(m);
		return 0;
	}
	if (length != 0) {
		/* A one-mbuf packet whose length the driver gives. */
		if (mbuf_next(m) == NULL)
			mbuf_setlen(m, length);
		mbuf_pkthdr_setlen(m, length);
	}
	len = (UInt32)mbuf_pkthdr_len(m);
	if (len < _headerLength || mbuf_len(m) < _headerLength) {
		ifnet_stat_increment_in(_ifp, 0, 0, 1);
		mbuf_freem(m);
		return 0;
	}
	mbuf_pkthdr_setrcvif(m, _ifp);

	/* The stack wants the link-level header apart from the payload. */
	void *header = mbuf_data(m);

	mbuf_pkthdr_setheader(m, header);
	mbuf_pkthdr_adjustlen(m, -(int)_headerLength);
	mbuf_setdata(m, (char *)header + _headerLength, mbuf_len(m) - _headerLength);
	bpf_tap_in(_ifp, getDataLinkType(), m, header, _headerLength);

	IOLockLock(_lock);
	if (_inputTail != NULL)
		mbuf_setnextpkt(_inputTail, m);
	else
		_inputHead = m;
	_inputTail = m;
	_inputCount++;
	_inputBytes += len;
	IOLockUnlock(_lock);

	if (options & kInputOptionQueuePacket)
		return 0;
	return flushInputQueue();
}

UInt32
IONetworkInterface::flushInputQueue()
{
	struct ifnet_stat_increment_param stats;
	mbuf_t head;
	UInt32 count;

	IOLockLock(_lock);
	head = _inputHead;
	count = _inputCount;
	bzero(&stats, sizeof(stats));
	stats.packets_in = _inputCount;
	stats.bytes_in = _inputBytes;
	_inputHead = _inputTail = NULL;
	_inputCount = _inputBytes = 0;
	IOLockUnlock(_lock);

	if (head != NULL)
		ifnet_input(_ifp, head, &stats);
	return count;
}

UInt32
IONetworkInterface::clearInputQueue()
{
	mbuf_t head;
	UInt32 count;

	if (_lock == NULL)
		return 0;
	IOLockLock(_lock);
	head = _inputHead;
	count = _inputCount;
	_inputHead = _inputTail = NULL;
	_inputCount = _inputBytes = 0;
	IOLockUnlock(_lock);
	if (head != NULL)
		mbuf_freem_list(head);
	return count;
}

/* Gated helpers: the controller's calls run on its work loop. */
static void
gatedEnable(void *target, void *ret, void *, void *, void *)
{
	IONetworkInterface *netif = (IONetworkInterface *)target;

	*(IOReturn *)ret = netif->getController()->enable(netif);
}

static void
gatedDisable(void *target, void *ret, void *, void *, void *)
{
	IONetworkInterface *netif = (IONetworkInterface *)target;

	*(IOReturn *)ret = netif->getController()->disable(netif);
}

static void
gatedUpdateFilters(void *target, void *ret, void *filters, void *, void *)
{
	IONetworkController *ctl = (IONetworkController *)target;

	*(IOReturn *)ret = ctl->updateFilters(*(UInt32 *)filters);
}

static void
gatedSelectMedium(void *target, void *ret, void *medium, void *, void *)
{
	IONetworkController *ctl = (IONetworkController *)target;

	*(IOReturn *)ret = ctl->selectMedium((const IONetworkMedium *)medium);
}

static int
errnoForIOReturn(IOReturn ret)
{
	switch (ret) {
	case kIOReturnSuccess:		return 0;
	case kIOReturnUnsupported:	return EOPNOTSUPP;
	case kIOReturnBadArgument:	return EINVAL;
	case kIOReturnNoMemory:		return ENOMEM;
	case kIOReturnNotPermitted:	return EPERM;
	default:			return EIO;
	}
}

/* IFF_UP turns the controller on and off; IFF_RUNNING follows. */
SInt32
IONetworkInterface::syncFlags()
{
	UInt16 flags = ifnet_flags(_ifp);
	IOReturn ret = kIOReturnSuccess;

	if ((flags & IFF_UP) && !_enabled) {
		_controller->executeCommand(this, gatedEnable, this, &ret);
		if (ret != kIOReturnSuccess) {
			IOLog("%s: enable failed (0x%x)\n", _controller->getName(), ret);
			return errnoForIOReturn(ret);
		}
		_enabled = true;
		if (_outputQueue != NULL)
			_outputQueue->start();
		ifnet_set_flags(_ifp, IFF_RUNNING, IFF_RUNNING);
	} else if (!(flags & IFF_UP) && _enabled) {
		_enabled = false;
		ifnet_set_flags(_ifp, 0, IFF_RUNNING);
		if (_outputQueue != NULL) {
			_outputQueue->stop();
			_outputQueue->flush();
		}
		_controller->executeCommand(this, gatedDisable, this, &ret);
		clearInputQueue();
	}
	setProperty(kIOInterfaceFlags, (UInt32)ifnet_flags(_ifp), 32);
	return 0;
}

SInt32
IONetworkInterface::setFilters(UInt32 filters)
{
	IOReturn ret = kIOReturnSuccess;

	if (filters == _filters)
		return 0;
	_controller->executeCommand(this, gatedUpdateFilters, _controller, &ret, &filters);
	_filters = filters;
	return errnoForIOReturn(ret);
}

void
IONetworkInterface::linkStatusChanged(UInt32 status, UInt64 speed)
{
	/* A kern_event_msg whose data (from event_data[0]) is a net_event_data. */
	UInt32 buf[(KEV_MSG_HEADER_SIZE + sizeof(struct net_event_data) + 3) / 4];
	struct kern_event_msg *ev = (struct kern_event_msg *)buf;
	struct net_event_data data;

	if (!_attached)
		return;
	ifnet_set_baudrate(_ifp, speed);
	if ((status & kIONetworkLinkValid) == 0)
		return;

	bzero(buf, sizeof(buf));
	bzero(&data, sizeof(data));
	ev->total_size = KEV_MSG_HEADER_SIZE + sizeof(data);
	ev->vendor_code = KEV_VENDOR_APPLE;
	ev->kev_class = KEV_NETWORK_CLASS;
	ev->kev_subclass = KEV_DL_SUBCLASS;
	ev->event_code = (status & kIONetworkLinkActive) ?
	    KEV_DL_LINK_ON : KEV_DL_LINK_OFF;
	data.if_family = ifnet_family(_ifp);
	data.if_unit = _unit;
	strlcpy(data.if_name, getNamePrefix(), sizeof(data.if_name));
	bcopy(&data, &ev->event_data[0], sizeof(data));
	ifnet_event(_ifp, ev);
}

/*
 * SIOCGIFMEDIA: the controller's selected and active media and the link
 * status. The list of media is not returned (ifm_count is 0).
 */
static SInt32
getMedia(IONetworkController *ctl, void *data)
{
	struct ifmediareq32 *ifmr = (struct ifmediareq32 *)data;
	const IONetworkMedium *selected = ctl->getSelectedMedium();
	UInt32 status = ctl->getLinkStatus();
	OSString *activeName;
	IONetworkMedium *active = NULL;

	activeName = OSDynamicCast(OSString, ctl->getProperty(kIOActiveMedium));
	if (activeName != NULL && ctl->getMediumDictionary() != NULL)
		active = OSDynamicCast(IONetworkMedium,
		    ctl->getMediumDictionary()->getObject(activeName));

	ifmr->ifm_current = selected != NULL ? (int)selected->getType() :
	    (active != NULL ? (int)active->getType() : 0);
	ifmr->ifm_mask = 0;
	ifmr->ifm_status = (int)(status & (IFM_AVALID | IFM_ACTIVE));
	ifmr->ifm_active = active != NULL ? (int)active->getType() : ifmr->ifm_current;
	ifmr->ifm_count = 0;
	return 0;
}

SInt32
IONetworkInterface::performCommand(IONetworkController *ctl, unsigned long cmd,
    void *, void *data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	UInt32 max = 0;
	IOReturn ret = kIOReturnSuccess;

	switch (cmd) {
	case SIOCSIFFLAGS:
		return syncFlags();

	case SIOCSIFMTU:
		if (ctl->getMaxPacketSize(&max) == kIOReturnSuccess &&
		    (UInt32)ifr->ifr_mtu + _headerLength + 4 > max)
			return EINVAL;
		setMaxTransferUnit((UInt32)ifr->ifr_mtu);
		return 0;

	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64:
		return getMedia(ctl, data);

	case SIOCSIFMEDIA: {
		IONetworkMedium *m = IONetworkMedium::getMediumWithType(
		    ctl->getMediumDictionary(), (IOMediumType)ifr->ifr_media);

		if (m == NULL)
			return EINVAL;
		ctl->executeCommand(this, gatedSelectMedium, ctl, &ret, m);
		return errnoForIOReturn(ret);
	}

	default:
		return EOPNOTSUPP;
	}
}

errno_t
IONetworkInterface::ifIoctl(ifnet_t ifp, unsigned long cmd, void *data)
{
	IONetworkInterface *netif = (IONetworkInterface *)ifnet_softc(ifp);

	if (netif == NULL || netif->_controller == NULL)
		return ENXIO;
	return netif->performCommand(netif->_controller, cmd, ifp, data);
}
