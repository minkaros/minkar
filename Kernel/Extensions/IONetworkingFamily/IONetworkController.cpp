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

#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSNumber.h>

const OSSymbol *gIONetworkFilterGroup;

#define super IOService
OSDefineMetaClassAndAbstractStructors(IONetworkController, IOService);
OSMetaClassDefineReservedUnused(IONetworkController, 0);
OSMetaClassDefineReservedUnused(IONetworkController, 1);
OSMetaClassDefineReservedUnused(IONetworkController, 2);
OSMetaClassDefineReservedUnused(IONetworkController, 3);
OSMetaClassDefineReservedUnused(IONetworkController, 4);
OSMetaClassDefineReservedUnused(IONetworkController, 5);
OSMetaClassDefineReservedUnused(IONetworkController, 6);
OSMetaClassDefineReservedUnused(IONetworkController, 7);

/* Creates the shared symbols; the first controller to start does it. */
void
IONetworkController::initialize()
{
	if (gIONetworkFilterGroup == NULL) {
		const OSSymbol *sym = OSSymbol::withCStringNoCopy(kIONetworkFilterGroup);

		if (sym != NULL && !OSCompareAndSwapPtr(NULL, __DECONST(void *, sym),
		    (void * volatile *)&gIONetworkFilterGroup))
			sym->release();
	}
}

bool
IONetworkController::init(OSDictionary *properties)
{
	initialize();
	if (!super::init(properties))
		return false;
	_filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast;
	return gIONetworkFilterGroup != NULL;
}

void
IONetworkController::free()
{
	OSSafeReleaseNULL(_outputQueue);
	if (_commandGate != NULL) {
		if (_workLoop != NULL)
			_workLoop->removeEventSource(_commandGate);
		OSSafeReleaseNULL(_commandGate);
	}
	OSSafeReleaseNULL(_workLoop);
	OSSafeReleaseNULL(_mediumDict);
	OSSafeReleaseNULL(_selectedMedium);
	super::free();
}

bool
IONetworkController::createWorkLoop()
{
	_workLoop = IOWorkLoop::workLoop();
	return _workLoop != NULL;
}

IOWorkLoop *
IONetworkController::getWorkLoop() const
{
	return _workLoop;
}

IOCommandGate *
IONetworkController::getCommandGate() const
{
	return _commandGate;
}

IOOutputQueue *
IONetworkController::createOutputQueue()
{
	return NULL;
}

IOOutputQueue *
IONetworkController::getOutputQueue() const
{
	return _outputQueue;
}

static void
setStringProperty(IOService *svc, const char *key, const OSString *value)
{
	if (value != NULL) {
		svc->setProperty(key, (OSObject *)value);
		value->release();
	}
}

bool
IONetworkController::start(IOService *provider)
{
	UInt32 size;

	if (!super::start(provider))
		return false;
	if (_workLoop == NULL && !createWorkLoop()) {
		IOLog("%s: no work loop\n", getName());
		return false;
	}
	_commandGate = IOCommandGate::commandGate(this);
	if (_commandGate == NULL ||
	    _workLoop->addEventSource(_commandGate) != kIOReturnSuccess) {
		IOLog("%s: no command gate\n", getName());
		return false;
	}
	_outputQueue = createOutputQueue();

	setStringProperty(this, kIOVendor, newVendorString());
	setStringProperty(this, kIOModel, newModelString());
	setStringProperty(this, kIORevision, newRevisionString());
	setProperty(kIOFeatures, getFeatures(), 32);
	if (getMaxPacketSize(&size) == kIOReturnSuccess)
		setProperty(kIOMaxPacketSize, size, 32);
	if (getMinPacketSize(&size) == kIOReturnSuccess)
		setProperty(kIOMinPacketSize, size, 32);
	return true;
}

void
IONetworkController::stop(IOService *provider)
{
	if (_outputQueue != NULL) {
		_outputQueue->stop();
		_outputQueue->flush();
	}
	super::stop(provider);
}

typedef IOReturn (*GateAction)(OSObject *, void *, void *, void *, void *);

struct ExecuteArgs {
	IONetworkController::Action action;
	void *target;
	void *param[4];
};

static IOReturn
executeInGate(OSObject *, void *arg, void *, void *, void *)
{
	ExecuteArgs *a = (ExecuteArgs *)arg;

	a->action(a->target, a->param[0], a->param[1], a->param[2], a->param[3]);
	return kIOReturnSuccess;
}

IOReturn
IONetworkController::executeCommand(OSObject *, Action action, void *target,
    void *param0, void *param1, void *param2, void *param3)
{
	ExecuteArgs args = { action, target, { param0, param1, param2, param3 } };

	if (_commandGate == NULL || action == NULL)
		return kIOReturnNotReady;
	return _commandGate->runAction(executeInGate, &args);
}

IOReturn
IONetworkController::enable(IONetworkInterface *)
{
	return kIOReturnSuccess;
}

IOReturn
IONetworkController::disable(IONetworkInterface *)
{
	return kIOReturnSuccess;
}

UInt32
IONetworkController::outputPacket(mbuf_t packet, void *)
{
	freePacket(packet);
	return kIOReturnOutputDropped;
}

IOReturn
IONetworkController::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
	if (filters == NULL)
		return kIOReturnBadArgument;
	if (group != gIONetworkFilterGroup)
		return kIOReturnUnsupported;
	*filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast;
	return kIOReturnSuccess;
}

IOReturn
IONetworkController::enablePacketFilter(const OSSymbol *, UInt32, UInt32, IOOptionBits)
{
	return kIOReturnUnsupported;
}

IOReturn
IONetworkController::disablePacketFilter(const OSSymbol *, UInt32, UInt32, IOOptionBits)
{
	return kIOReturnUnsupported;
}

/*
 * The interface wants these kIONetworkFilterGroup filters: enable and
 * disable the ones that changed, one at a time, as on macOS.
 */
IOReturn
IONetworkController::updateFilters(UInt32 filters)
{
	UInt32 supported = 0, changed;
	IOReturn ret = kIOReturnSuccess;

	getPacketFilters(gIONetworkFilterGroup, &supported);
	filters &= supported;
	changed = filters ^ _filters;
	for (UInt32 bit = 1; changed != 0 && bit != 0; bit <<= 1) {
		IOReturn r;

		if ((changed & bit) == 0)
			continue;
		changed &= ~bit;
		if (filters & bit)
			r = enablePacketFilter(gIONetworkFilterGroup, bit, _filters | bit);
		else
			r = disablePacketFilter(gIONetworkFilterGroup, bit, _filters & ~bit);
		if (r == kIOReturnSuccess)
			_filters ^= bit;
		else
			ret = r;
	}
	setProperty(kIOPacketFilters, _filters, 32);
	return ret;
}

IOReturn
IONetworkController::selectMedium(const IONetworkMedium *)
{
	return kIOReturnUnsupported;
}

IOReturn
IONetworkController::getChecksumSupport(UInt32 *checksumMask, UInt32, bool)
{
	if (checksumMask != NULL)
		*checksumMask = 0;
	return kIOReturnUnsupported;
}

UInt32 IONetworkController::getFeatures() const { return 0; }
const OSString *IONetworkController::newVendorString() const { return NULL; }
const OSString *IONetworkController::newModelString() const { return NULL; }
const OSString *IONetworkController::newRevisionString() const { return NULL; }

bool
IONetworkController::publishProperties()
{
	return true;
}

/* The interface sends through the controller's output queue. */
bool
IONetworkController::configureInterface(IONetworkInterface *netif)
{
	if (netif == NULL || netif->getController() != this)
		return false;
	netif->_outputQueue = _outputQueue;
	return true;
}

bool
IONetworkController::attachInterface(IONetworkInterface **interfaceP, bool doRegister)
{
	IONetworkInterface *netif;

	if (interfaceP == NULL)
		return false;
	*interfaceP = NULL;
	if (_interface != NULL) {
		IOLog("%s: an interface is already attached\n", getName());
		return false;
	}
	if ((netif = createInterface()) == NULL)
		return false;
	if (!configureInterface(netif) || !netif->attach(this)) {
		netif->release();
		return false;
	}
	_interface = netif;
	publishProperties();
	if (doRegister)
		netif->registerService();
	*interfaceP = netif;	/* the caller owns the reference */
	return true;
}

void
IONetworkController::detachInterface(IONetworkInterface *netif, bool sync)
{
	if (netif == NULL || netif != _interface)
		return;
	netif->detachFromDataLinkLayer(0, NULL);
	if (sync)
		netif->terminate(kIOServiceSynchronous);
	else
		netif->terminate();
	_interface = NULL;
}

bool
IONetworkController::publishMediumDictionary(const OSDictionary *mediumDict)
{
	OSDictionary *copy;

	if (mediumDict == NULL)
		return false;
	if ((copy = OSDictionary::withDictionary(mediumDict)) == NULL)
		return false;
	OSSafeReleaseNULL(_mediumDict);
	_mediumDict = copy;
	setProperty(kIOMediumDictionary, _mediumDict);
	return true;
}

const OSDictionary *
IONetworkController::getMediumDictionary() const
{
	return _mediumDict;
}

OSDictionary *
IONetworkController::copyMediumDictionary() const
{
	return _mediumDict != NULL ? OSDictionary::withDictionary(_mediumDict) : NULL;
}

bool
IONetworkController::setSelectedMedium(const IONetworkMedium *medium)
{
	if (medium != NULL)
		medium->retain();
	OSSafeReleaseNULL(_selectedMedium);
	_selectedMedium = medium;
	if (medium != NULL)
		setProperty(kIOSelectedMedium, (OSObject *)medium->getName());
	else
		removeProperty(kIOSelectedMedium);
	return true;
}

const IONetworkMedium *
IONetworkController::getSelectedMedium() const
{
	return _selectedMedium;
}

UInt32 IONetworkController::getLinkStatus() const { return _linkStatus; }
UInt64 IONetworkController::getLinkSpeed() const { return _linkSpeed; }

bool
IONetworkController::setLinkStatus(UInt32 status, const IONetworkMedium *activeMedium,
    UInt64 speed, OSData *data)
{
	bool changed;

	if (activeMedium != NULL && speed == 0)
		speed = activeMedium->getSpeed();
	if ((status & kIONetworkLinkActive) == 0)
		speed = 0;
	changed = status != _linkStatus || speed != _linkSpeed;
	_linkStatus = status;
	_linkSpeed = speed;

	setProperty(kIOLinkStatus, status, 32);
	setProperty(kIOLinkSpeed, speed, 64);
	if (activeMedium != NULL)
		setProperty(kIOActiveMedium, (OSObject *)activeMedium->getName());
	else
		removeProperty(kIOActiveMedium);
	if (data != NULL)
		setProperty(kIOLinkData, data);
	else
		removeProperty(kIOLinkData);

	if (changed && _interface != NULL)
		_interface->linkStatusChanged(status, speed);
	return true;
}

/*
 * A packet of size bytes in as few mbufs as possible (one cluster for a
 * normal Ethernet frame). Usable from the work loop: it never sleeps.
 */
mbuf_t
IONetworkController::allocatePacket(UInt32 size)
{
	unsigned int chunks = 1;
	mbuf_t m = NULL, n;
	size_t left = size;

	if (size == 0)
		return NULL;
	if (mbuf_allocpacket(MBUF_DONTWAIT, size, &chunks, &m) != 0 &&
	    mbuf_allocpacket(MBUF_DONTWAIT, size, NULL, &m) != 0)
		return NULL;
	for (n = m; n != NULL; n = mbuf_next(n)) {
		size_t len = mbuf_maxlen(n);

		if (len > left)
			len = left;
		mbuf_setlen(n, len);
		left -= len;
	}
	mbuf_pkthdr_setlen(m, size);
	return m;
}

void
IONetworkController::freePacket(mbuf_t packet, IOOptionBits)
{
	if (packet != NULL)
		mbuf_freem(packet);
}

mbuf_t
IONetworkController::copyPacket(const mbuf_t packet, UInt32 size)
{
	mbuf_t m = NULL;

	if (packet == NULL)
		return NULL;
	if (size == 0)
		size = (UInt32)mbuf_pkthdr_len(packet);
	if ((m = allocatePacket(size)) != NULL && mbuf_next(m) == NULL &&
	    mbuf_copydata(packet, 0, size, mbuf_data(m)) == 0)
		return m;
	/* Packets that need several mbufs go through mbuf_dup. */
	if (m != NULL)
		mbuf_freem(m);
	m = NULL;
	if (mbuf_dup(packet, MBUF_DONTWAIT, &m) != 0)
		return NULL;
	return m;
}
