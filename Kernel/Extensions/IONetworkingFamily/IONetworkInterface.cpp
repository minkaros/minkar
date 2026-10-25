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

#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkStack.h>
#include <IOKit/network/IONetworkData.h>
#include <IOKit/network/IONetworkStats.h>
#include <IOKit/network/IONetworkUserClient.h>
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

bool
IONetworkInterface::init(IONetworkController *controller)
{
	if (controller == NULL || !super::init())
		return false;
	_controller = controller;
	_lock = IOLockAlloc();
	_mtu = 1500;
	_filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast;
	_dataList = OSArray::withCapacity(4);
	_dataDict = OSDictionary::withCapacity(4);
	if (_lock == NULL || _dataList == NULL || _dataDict == NULL)
		return false;
	setProperty(kIOInterfaceState, (unsigned long long)0, 32);

	/* Generic statistics, and the output queue's. */
	IONetworkData *stats = IONetworkData::withInternalBuffer(kIONetworkStatsKey,
	    sizeof(IONetworkStats));
	if (stats == NULL || !addNetworkData(stats)) {
		OSSafeReleaseNULL(stats);
		return false;
	}
	_stats = (IONetworkStats *)stats->getBuffer();
	stats->release();
	IONetworkData *qstats = IONetworkData::withInternalBuffer(kIOOutputQueueStatsKey,
	    sizeof(IOOutputQueueStats), kIONetworkDataBasicAccessTypes, this,
	    outputQueueStatsAction);
	if (qstats != NULL) {
		addNetworkData(qstats);
		qstats->release();
	}
	return true;
}

/* The output queue's counters, fetched when the data is read. */
IOReturn
IONetworkInterface::outputQueueStatsAction(void *target, void *, IONetworkData *data,
    UInt32, void *, UInt32 *, UInt32)
{
	IONetworkInterface *netif = (IONetworkInterface *)target;
	IOOutputQueueStats *q = (IOOutputQueueStats *)data->getBuffer();
	IOOutputQueue *queue = netif->_outputQueue;

	if (queue != NULL && q != NULL) {
		q->capacity = queue->getCapacity();
		q->size = queue->getSize();
		if (q->size > q->peakSize)
			q->peakSize = q->size;
		q->dropCount = queue->getDropCount();
		q->outputCount = queue->getOutputCount();
		q->retryCount = queue->getRetryCount();
		q->stallCount = queue->getStallCount();
	}
	return kIOReturnSuccess;
}

/* ---- Network data -------------------------------------------------------- */

bool
IONetworkInterface::addNetworkData(IONetworkData *data)
{
	bool ok;

	if (data == NULL || data->getKey() == NULL)
		return false;
	IOLockLock(_lock);
	ok = _dataDict->getObject(data->getKey()) == NULL &&
	    _dataList->setObject(data) && _dataDict->setObject(data->getKey(), data);
	IOLockUnlock(_lock);
	if (ok)
		setProperty("IONetworkData", _dataDict);
	return ok;
}

bool
IONetworkInterface::removeNetworkData(const OSSymbol *key)
{
	bool found = false;

	IOLockLock(_lock);
	for (unsigned i = 0; i < _dataList->getCount(); i++) {
		IONetworkData *d = (IONetworkData *)_dataList->getObject(i);

		if (d->getKey() == key) {
			/* Handles are indexes: leave a hole rather than shift them. */
			_dataList->replaceObject(i, kOSBooleanFalse);
			_dataDict->removeObject(key);
			found = true;
			break;
		}
	}
	IOLockUnlock(_lock);
	if (found)
		setProperty("IONetworkData", _dataDict);
	return found;
}

IONetworkData *
IONetworkInterface::getNetworkData(const OSSymbol *key) const
{
	return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

IONetworkData *
IONetworkInterface::getNetworkData(const char *key) const
{
	return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

/* User clients name data by handle: its index in _dataList. */
bool
IONetworkInterface::getNetworkDataHandle(const char *key, UInt32 *handle) const
{
	IONetworkData *d = getNetworkData(key);

	if (d == NULL)
		return false;
	for (unsigned i = 0; i < _dataList->getCount(); i++) {
		if (_dataList->getObject(i) == d) {
			*handle = i;
			return true;
		}
	}
	return false;
}

IONetworkData *
IONetworkInterface::getNetworkDataForHandle(UInt32 handle) const
{
	return OSDynamicCast(IONetworkData, _dataList->getObject(handle));
}

IONetworkStats *
IONetworkInterface::getNetworkStats() const
{
	return _stats;
}

/*
 * The driver counts its errors in IONetworkStats; the deltas since the last
 * call go to the ifnet's counters (netstat -i).
 */
void
IONetworkInterface::syncErrorStats()
{
	struct ifnet_stat_increment_param inc;
	UInt32 ie, oe, col;

	if (_stats == NULL || _ifp == NULL)
		return;
	ie = _stats->inputErrors;
	oe = _stats->outputErrors;
	col = _stats->collisions;
	if (ie == _lastInputErrors && oe == _lastOutputErrors && col == _lastCollisions)
		return;
	bzero(&inc, sizeof(inc));
	inc.errors_in = ie - _lastInputErrors;
	inc.errors_out = oe - _lastOutputErrors;
	inc.collisions = col - _lastCollisions;
	_lastInputErrors = ie;
	_lastOutputErrors = oe;
	_lastCollisions = col;
	ifnet_stat_increment(_ifp, &inc);
}

IOReturn
IONetworkInterface::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	IONetworkUserClient *client;

	if (type != kIONUCType)
		return kIOReturnBadArgument;
	if ((client = IONetworkUserClient::withTask(owningTask, securityID)) == NULL)
		return kIOReturnNoMemory;
	if (!client->attach(this) || !client->start(this)) {
		client->detach(this);
		client->release();
		return kIOReturnError;
	}
	*handler = client;
	return kIOReturnSuccess;
}

/* IONetworkLib's IONetworkSetPacketFiltersMask: { IORequiredPacketFilters: { group: mask } }. */
IOReturn
IONetworkInterface::setProperties(OSObject *properties)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
	OSDictionary *required;
	OSNumber *mask;

	if (dict == NULL)
		return kIOReturnBadArgument;
	required = OSDynamicCast(OSDictionary, dict->getObject("IORequiredPacketFilters"));
	if (required == NULL)
		return kIOReturnUnsupported;
	if ((mask = OSDynamicCast(OSNumber, required->getObject(kIONetworkFilterGroup))) != NULL) {
		UInt32 f = mask->unsigned32BitValue() | kIOPacketFilterUnicast |
		    kIOPacketFilterBroadcast;
		SInt32 err = setFilters(f);

		if (err != 0)
			return kIOReturnError;
	}
	return kIOReturnSuccess;
}

void
IONetworkInterface::free()
{
	clearInputQueue();
	if (_ifp != NULL) {
		IONetworkStack::releaseUnit(getNamePrefix(), _unit);
		ifnet_release(_ifp);
		_ifp = NULL;
	}
	OSSafeReleaseNULL(_dataList);
	OSSafeReleaseNULL(_dataDict);
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
	configureOffloads();

	snprintf(bsdName, sizeof(bsdName), "%s%u", getNamePrefix(), _unit);
	setProperty(kIOBSDNameKey, bsdName);
	setProperty(kIOInterfaceUnit, _unit, 32);
	setProperty(kIOPrimaryInterface, _unit == 0);
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

/* Called by IONetworkStack once the interface has its unit. */
bool
IONetworkInterface::attachToDataLinkLayerWithUnit(UInt32 unit)
{
	if (_attached)
		return _unit == unit;
	_unit = unit;
	return attachToDataLinkLayer(0, NULL);
}

/*
 * Tells the stack what the controller does in hardware: checksums (from
 * getChecksumSupport), TSO and VLAN tagging (from getFeatures).
 */
void
IONetworkInterface::configureOffloads()
{
	UInt32 csum = 0, features = _controller->getFeatures();
	ifnet_offload_t offload = 0;

	if (_controller->getChecksumSupport(&csum, kChecksumFamilyTCPIP, true) != kIOReturnSuccess)
		csum = 0;
	if (csum & kChecksumIP)
		offload |= IFNET_CSUM_IP;
	if (csum & kChecksumTCP)
		offload |= IFNET_CSUM_TCP;
	if (csum & kChecksumUDP)
		offload |= IFNET_CSUM_UDP;
	if (csum & kChecksumTCPIPv6)
		offload |= IFNET_CSUM_TCPIPV6;
	if (csum & kChecksumUDPIPv6)
		offload |= IFNET_CSUM_UDPIPV6;
	if (features & kIONetworkFeatureHardwareVlan)
		offload |= IFNET_VLAN_TAGGING;
	if (features & (kIONetworkFeatureHardwareVlan | kIONetworkFeatureSoftwareVlan))
		offload |= IFNET_VLAN_MTU;
	if (features & kIONetworkFeatureTSOIPv4) {
		offload |= IFNET_TSO_IPV4;
		ifnet_set_tso_mtu(_ifp, AF_INET, 65535);
	}
	if (features & kIONetworkFeatureTSOIPv6) {
		offload |= IFNET_TSO_IPV6;
		ifnet_set_tso_mtu(_ifp, AF_INET6, 65535);
	}
	if (offload != 0)
		ifnet_set_offload(_ifp, offload);
}

/*
 * Publishes the interface; it gets its BSD interface (and unit) when the
 * IONetworkStack names it.
 */
void
IONetworkInterface::registerService(IOOptionBits options)
{
	setProperty(kIOInterfaceNamePrefix, getNamePrefix());
	setProperty(kIOBuiltin, true);
	publishRequiredFilters();
	super::registerService(options);
	IONetworkStack::interfacePublished(this);
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
		if (netif->_stats != NULL)
			netif->_stats->outputPackets++;
	}
	netif->syncErrorStats();
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
		if (_stats != NULL)
			_stats->inputErrors++;
		mbuf_freem(m);
		return 0;
	}
	if (_stats != NULL)
		_stats->inputPackets++;
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
	syncErrorStats();
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
	publishRequiredFilters();
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
 * SIOCGIFMEDIA: the controller's selected and active media, the link
 * status, and as many of its media types as the caller's list holds
 * (ifm_count says how many there are).
 */
/* The smallest MTU IPv4 allows. */
static const int kMinMTU = 68;

static SInt32
getMedia(IONetworkController *ctl, unsigned long cmd, void *data)
{
	struct ifmediareq32 *ifmr = (struct ifmediareq32 *)data;
	const IONetworkMedium *selected = ctl->getSelectedMedium();
	const OSDictionary *media = ctl->getMediumDictionary();
	UInt32 status = ctl->getLinkStatus();
	OSString *activeName;
	IONetworkMedium *active = NULL;
	user_addr_t ulist;
	int total = media != NULL ? (int)media->getCount() : 0;

	activeName = OSDynamicCast(OSString, ctl->getProperty(kIOActiveMedium));
	if (activeName != NULL && media != NULL)
		active = OSDynamicCast(IONetworkMedium, media->getObject(activeName));

	ifmr->ifm_current = selected != NULL ? (int)selected->getType() :
	    (active != NULL ? (int)active->getType() : 0);
	ifmr->ifm_mask = 0;
	ifmr->ifm_status = (int)(status & (IFM_AVALID | IFM_ACTIVE));
	ifmr->ifm_active = active != NULL ? (int)active->getType() : ifmr->ifm_current;

	/* The two layouts differ only in the pointer's size. */
	ulist = cmd == SIOCGIFMEDIA64 ?
	    (user_addr_t)((struct ifmediareq64 *)data)->ifmu_ulist :
	    CAST_USER_ADDR_T(ifmr->ifmu_ulist);
	if (ifmr->ifm_count > 0 && ulist != USER_ADDR_NULL && media != NULL) {
		OSCollectionIterator *iter = OSCollectionIterator::withCollection(media);
		const OSSymbol *key;
		int n = 0;

		while (iter != NULL && n < ifmr->ifm_count &&
		    (key = OSDynamicCast(OSSymbol, iter->getNextObject())) != NULL) {
			IONetworkMedium *m = OSDynamicCast(IONetworkMedium, media->getObject(key));
			int word;

			if (m == NULL)
				continue;
			word = (int)m->getType();
			if (copyout(&word, ulist + (user_addr_t)n * sizeof(word), sizeof(word)) != 0) {
				OSSafeReleaseNULL(iter);
				return EFAULT;
			}
			n++;
		}
		OSSafeReleaseNULL(iter);
	}
	ifmr->ifm_count = total;
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
		if (ifr->ifr_mtu < kMinMTU ||
		    (ctl->getMaxPacketSize(&max) == kIOReturnSuccess &&
		    (UInt32)ifr->ifr_mtu + _headerLength + 4 > max))
			return EINVAL;
		setMaxTransferUnit((UInt32)ifr->ifr_mtu);
		return 0;

	case SIOCGIFDEVMTU:
		/* The MTU range the controller's packet size allows (VLAN asks). */
		ifr->ifr_devmtu.ifdm_current = (int)ifnet_mtu(_ifp);
		ifr->ifr_devmtu.ifdm_min = kMinMTU;
		ifr->ifr_devmtu.ifdm_max = ctl->getMaxPacketSize(&max) == kIOReturnSuccess &&
		    max > _headerLength + 4 ? (int)(max - _headerLength - 4) :
		    (int)ifnet_mtu(_ifp);
		return 0;

	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64:
		return getMedia(ctl, cmd, data);

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

/* { IONetworkFilterGroup: the filters the interface wants } */
OSDictionary *
IONetworkInterface::_requiredFilters()
{
	OSDictionary *dict = OSDictionary::withCapacity(1);
	OSNumber *num = OSNumber::withNumber(_filters, 32);

	if (dict != NULL && num != NULL)
		dict->setObject(kIONetworkFilterGroup, num);
	OSSafeReleaseNULL(num);
	return dict;		/* retained */
}

void
IONetworkInterface::publishRequiredFilters()
{
	OSDictionary *dict = _requiredFilters();

	if (dict != NULL) {
		setProperty("IORequiredPacketFilters", dict);
		dict->release();
	}
}
