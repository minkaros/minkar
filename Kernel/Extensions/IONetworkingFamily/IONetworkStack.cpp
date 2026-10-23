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

#include <IOKit/network/IONetworkStack.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSSet.h>
#include <libkern/OSAtomic.h>
#include <net/if.h>

#define super IOService
OSDefineMetaClassAndStructors(IONetworkStack, IOService);

static IONetworkStack *gStack;
static OSSet *gPending;		/* interfaces published before the stack ran */
static IOLock *gStackLock;

/* ---- Unit numbers ------------------------------------------------------- */

#define MAX_PREFIXES	8
#define MAX_UNITS	128
static struct {
	char		prefix[IFNAMSIZ];
	uint64_t	used[MAX_UNITS / 64];
} gUnits[MAX_PREFIXES];

static IOLock *
stackLock(void)
{
	if (gStackLock == NULL) {
		IOLock *lock = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&gStackLock))
			IOLockFree(lock);
	}
	return gStackLock;
}

/* The unit table of prefix, made if needed (call with the lock held). */
static uint64_t *
unitsFor(const char *prefix)
{
	for (int i = 0; i < MAX_PREFIXES; i++) {
		if (gUnits[i].prefix[0] == '\0')
			strlcpy(gUnits[i].prefix, prefix, sizeof(gUnits[i].prefix));
		if (strncmp(gUnits[i].prefix, prefix, sizeof(gUnits[i].prefix)) == 0)
			return gUnits[i].used;
	}
	return NULL;
}

bool
IONetworkStack::reserveUnit(const char *prefix, UInt32 unit)
{
	uint64_t *used;
	bool ok = false;

	if (unit >= MAX_UNITS)
		return false;
	IOLockLock(stackLock());
	if ((used = unitsFor(prefix)) != NULL && !(used[unit / 64] & (1ULL << (unit % 64)))) {
		used[unit / 64] |= 1ULL << (unit % 64);
		ok = true;
	}
	IOLockUnlock(stackLock());
	return ok;
}

bool
IONetworkStack::lowestFreeUnit(const char *prefix, UInt32 from, UInt32 *unit)
{
	uint64_t *used;
	bool ok = false;

	IOLockLock(stackLock());
	if ((used = unitsFor(prefix)) != NULL) {
		for (UInt32 u = from; u < MAX_UNITS; u++) {
			if (!(used[u / 64] & (1ULL << (u % 64)))) {
				used[u / 64] |= 1ULL << (u % 64);
				*unit = u;
				ok = true;
				break;
			}
		}
	}
	IOLockUnlock(stackLock());
	return ok;
}

void
IONetworkStack::releaseUnit(const char *prefix, UInt32 unit)
{
	uint64_t *used;

	if (unit >= MAX_UNITS)
		return;
	IOLockLock(stackLock());
	if ((used = unitsFor(prefix)) != NULL)
		used[unit / 64] &= ~(1ULL << (unit % 64));
	IOLockUnlock(stackLock());
}

/* ---- The service -------------------------------------------------------- */

bool
IONetworkStack::start(IOService *provider)
{
	OSBoolean *autoReg;
	OSNumber *timeout;
	OSSet *pending;
	uint64_t deadline;

	if (!super::start(provider))
		return false;
	_lock = IOLockAlloc();
	_interfaces = OSSet::withCapacity(4);
	_namerTimer = thread_call_allocate(namerTimeout, this);
	if (_lock == NULL || _interfaces == NULL || _namerTimer == NULL)
		return false;

	autoReg = OSDynamicCast(OSBoolean, getProperty("IONetworkStackAutoRegister"));
	_autoRegister = autoReg != NULL && autoReg->isTrue();
	timeout = OSDynamicCast(OSNumber, getProperty("IONetworkStackNamerTimeout"));

	IOLockLock(stackLock());
	gStack = this;
	pending = gPending;
	gPending = NULL;
	IOLockUnlock(stackLock());
	if (pending != NULL) {
		IOLockLock(_lock);
		_interfaces->merge(pending);
		IOLockUnlock(_lock);
		pending->release();
	}

	registerService();
	if (_autoRegister) {
		registerAllInterfaces();
	} else {
		retain();
		clock_interval_to_deadline(timeout != NULL ? timeout->unsigned32BitValue() : 30,
		    kSecondScale, &deadline);
		if (thread_call_enter_delayed(_namerTimer, deadline))
			release();
	}
	return true;
}

void
IONetworkStack::free()
{
	if (_namerTimer != NULL) {
		thread_call_cancel(_namerTimer);
		thread_call_free(_namerTimer);
	}
	OSSafeReleaseNULL(_interfaces);
	if (_lock != NULL)
		IOLockFree(_lock);
	super::free();
}

/* No namer in time (configd not running): name in order of appearance. */
void
IONetworkStack::namerTimeout(thread_call_param_t param, thread_call_param_t)
{
	IONetworkStack *stack = (IONetworkStack *)param;

	if (!stack->_namerSeen) {
		IOLog("IONetworkStack: no interface namer; naming interfaces in order\n");
		stack->_autoRegister = true;
		stack->registerAllInterfaces();
	}
	stack->release();
}

void
IONetworkStack::namerConnected()
{
	if (!_namerSeen) {
		_namerSeen = true;
		if (thread_call_cancel(_namerTimer))
			release();
	}
}

void
IONetworkStack::interfacePublished(IONetworkInterface *netif)
{
	IONetworkStack *stack;

	IOLockLock(stackLock());
	stack = gStack;
	if (stack == NULL) {
		if (gPending == NULL)
			gPending = OSSet::withCapacity(4);
		if (gPending != NULL)
			gPending->setObject(netif);
		IOLockUnlock(stackLock());
		return;
	}
	stack->retain();
	IOLockUnlock(stackLock());

	IOLockLock(stack->_lock);
	stack->_interfaces->setObject(netif);
	IOLockUnlock(stack->_lock);
	if (stack->_autoRegister)
		stack->registerInterface(netif, 0, false);
	stack->release();
}

IOReturn
IONetworkStack::registerInterface(IONetworkInterface *netif, UInt32 unit, bool fixedUnit)
{
	const char *prefix = netif->getNamePrefix();
	UInt32 u = unit;

	if (netif->getIfnet() != NULL)
		return netif->getUnitNumber() == unit || !fixedUnit ? kIOReturnSuccess : kIOReturnBusy;
	if (fixedUnit ? !reserveUnit(prefix, unit) : !lowestFreeUnit(prefix, unit, &u)) {
		IOLog("IONetworkStack: %s%u is taken\n", prefix, unit);
		return kIOReturnBusy;
	}
	if (!netif->attachToDataLinkLayerWithUnit(u)) {
		releaseUnit(prefix, u);
		return kIOReturnError;
	}
	IOLockLock(_lock);
	_interfaces->removeObject(netif);
	IOLockUnlock(_lock);
	return kIOReturnSuccess;
}

IOReturn
IONetworkStack::registerAllInterfaces()
{
	OSArray *list;
	OSCollectionIterator *iter;
	IONetworkInterface *netif;

	IOLockLock(_lock);
	list = OSArray::withCapacity(_interfaces->getCount());
	if (list != NULL && (iter = OSCollectionIterator::withCollection(_interfaces)) != NULL) {
		while ((netif = OSDynamicCast(IONetworkInterface, iter->getNextObject())) != NULL)
			list->setObject(netif);
		iter->release();
	}
	IOLockUnlock(_lock);
	if (list == NULL)
		return kIOReturnNoMemory;
	for (unsigned i = 0; i < list->getCount(); i++)
		registerInterface((IONetworkInterface *)list->getObject(i), 0, false);
	list->release();
	return kIOReturnSuccess;
}

/* The interface a command names, by registry entry ID or by path. */
IONetworkInterface *
IONetworkStack::findInterface(OSDictionary *command)
{
	OSObject *idObj = command->getObject("IORegistryEntryID");
	OSString *path = OSDynamicCast(OSString, command->getObject("IOPathMatch"));
	IONetworkInterface *found = NULL;
	uint64_t entryID = 0;

	if (OSData *d = OSDynamicCast(OSData, idObj)) {
		if (d->getLength() == sizeof(entryID))
			bcopy(d->getBytesNoCopy(), &entryID, sizeof(entryID));
	} else if (OSNumber *n = OSDynamicCast(OSNumber, idObj)) {
		entryID = n->unsigned64BitValue();
	}
	if (entryID != 0) {
		OSCollectionIterator *iter;

		IOLockLock(_lock);
		if ((iter = OSCollectionIterator::withCollection(_interfaces)) != NULL) {
			IONetworkInterface *netif;

			while ((netif = OSDynamicCast(IONetworkInterface, iter->getNextObject())) != NULL) {
				if (netif->getRegistryEntryID() == entryID) {
					found = netif;
					found->retain();
					break;
				}
			}
			iter->release();
		}
		IOLockUnlock(_lock);
	} else if (path != NULL) {
		IORegistryEntry *entry = IORegistryEntry::fromPath(path->getCStringNoCopy());

		if ((found = OSDynamicCast(IONetworkInterface, entry)) == NULL && entry != NULL)
			entry->release();
	}
	return found;		/* retained */
}

IOReturn
IONetworkStack::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	IONetworkStackUserClient *client;

	if (IOUserClient::clientHasPrivilege(securityID, kIOClientPrivilegeAdministrator) !=
	    kIOReturnSuccess)
		return kIOReturnNotPrivileged;
	client = new IONetworkStackUserClient;
	if (client == NULL || !client->initWithTask(owningTask, securityID, type) ||
	    !client->attach(this) || !client->start(this)) {
		if (client != NULL) {
			client->detach(this);
			client->release();
		}
		return kIOReturnNoMemory;
	}
	namerConnected();
	*handler = client;
	return kIOReturnSuccess;
}

/* ---- The user client ---------------------------------------------------- */

#undef super
#define super IOUserClient
OSDefineMetaClassAndStructors(IONetworkStackUserClient, IOUserClient);

bool
IONetworkStackUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	return super::initWithTask(owningTask, securityID, type);
}

bool
IONetworkStackUserClient::start(IOService *provider)
{
	if (!super::start(provider))
		return false;
	_stack = OSDynamicCast(IONetworkStack, provider);
	return _stack != NULL;
}

IOReturn
IONetworkStackUserClient::clientClose()
{
	detach(_stack);
	return kIOReturnSuccess;
}

/* { IONetworkStackUserCommand, IORegistryEntryID | IOPathMatch, IOInterfaceUnit } */
IOReturn
IONetworkStackUserClient::setProperties(OSObject *properties)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
	OSNumber *cmd, *unit;
	IONetworkInterface *netif;
	IOReturn ret;

	if (dict == NULL || _stack == NULL)
		return kIOReturnBadArgument;
	cmd = OSDynamicCast(OSNumber, dict->getObject(kIONetworkStackUserCommandKey));
	unit = OSDynamicCast(OSNumber, dict->getObject(kIOInterfaceUnit));
	if (cmd == NULL)
		return kIOReturnBadArgument;
	switch (cmd->unsigned32BitValue()) {
	case kIONetworkStackRegisterInterfaceAll:
		return _stack->registerAllInterfaces();
	case kIONetworkStackRegisterInterfaceWithUnit:
	case kIONetworkStackRegisterInterfaceWithLowestUnit:
		if (unit == NULL || (netif = _stack->findInterface(dict)) == NULL)
			return kIOReturnBadArgument;
		ret = _stack->registerInterface(netif, unit->unsigned32BitValue(),
		    cmd->unsigned32BitValue() == kIONetworkStackRegisterInterfaceWithUnit);
		netif->release();
		return ret;
	default:
		return kIOReturnUnsupported;
	}
}

IOService *
IONetworkStackUserClient::getService()
{
	return _stack;
}
