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

#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/c++/OSCollectionIterator.h>

#define super OSObject
OSDefineMetaClassAndStructors(IONetworkMedium, OSObject);
OSMetaClassDefineReservedUnused(IONetworkMedium, 0);
OSMetaClassDefineReservedUnused(IONetworkMedium, 1);
OSMetaClassDefineReservedUnused(IONetworkMedium, 2);
OSMetaClassDefineReservedUnused(IONetworkMedium, 3);

const OSSymbol *
IONetworkMedium::nameForType(IOMediumType type)
{
	char name[12];

	snprintf(name, sizeof(name), "%08x", (unsigned int)type);
	return OSSymbol::withCString(name);
}

IONetworkMedium *
IONetworkMedium::medium(IOMediumType type, UInt64 speed, UInt32 flags,
    UInt32 index, const char *name)
{
	IONetworkMedium *m = new IONetworkMedium;

	if (m != NULL && !m->init(type, speed, flags, index, name)) {
		m->release();
		m = NULL;
	}
	return m;
}

bool
IONetworkMedium::init(IOMediumType type, UInt64 speed, UInt32 flags,
    UInt32 index, const char *name)
{
	if (!super::init())
		return false;
	_type = type;
	_speed = speed;
	_flags = flags;
	_index = index;
	_name = name != NULL ? OSSymbol::withCString(name) : nameForType(type);
	return _name != NULL;
}

void
IONetworkMedium::free()
{
	OSSafeReleaseNULL(_name);
	super::free();
}

IOMediumType IONetworkMedium::getType() const { return _type; }
UInt64 IONetworkMedium::getSpeed() const { return _speed; }
UInt32 IONetworkMedium::getFlags() const { return _flags; }
UInt32 IONetworkMedium::getIndex() const { return _index; }
const OSSymbol *IONetworkMedium::getName() const { return _name; }
const OSSymbol *IONetworkMedium::getKey() const { return _name; }

bool
IONetworkMedium::isEqualTo(const IONetworkMedium *m) const
{
	return m != NULL && m->_type == _type && m->_speed == _speed &&
	    m->_flags == _flags && m->_index == _index && m->_name == _name;
}

bool
IONetworkMedium::isEqualTo(const OSMetaClassBase *obj) const
{
	return isEqualTo(OSDynamicCast(IONetworkMedium, obj));
}

/* A medium serializes as a dictionary: Type, Speed, Flags and Index. */
bool
IONetworkMedium::serialize(OSSerialize *s) const
{
	OSDictionary *dict = OSDictionary::withCapacity(4);
	OSNumber *num;
	bool ok;

	if (dict == NULL)
		return false;
	if ((num = OSNumber::withNumber(_type, 32)) != NULL) {
		dict->setObject(kIOMediumType, num);
		num->release();
	}
	if ((num = OSNumber::withNumber(_speed, 64)) != NULL) {
		dict->setObject(kIOMediumSpeed, num);
		num->release();
	}
	if ((num = OSNumber::withNumber(_flags, 32)) != NULL) {
		dict->setObject(kIOMediumFlags, num);
		num->release();
	}
	if ((num = OSNumber::withNumber(_index, 32)) != NULL) {
		dict->setObject(kIOMediumIndex, num);
		num->release();
	}
	ok = dict->serialize(s);
	dict->release();
	return ok;
}

IOReturn
IONetworkMedium::addMedium(OSDictionary *dict, const IONetworkMedium *m)
{
	if (dict == NULL || m == NULL)
		return kIOReturnBadArgument;
	return dict->setObject(m->getKey(), m) ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn
IONetworkMedium::removeMedium(OSDictionary *dict, const IONetworkMedium *m)
{
	if (dict == NULL || m == NULL)
		return kIOReturnBadArgument;
	dict->removeObject(m->getKey());
	return kIOReturnSuccess;
}

/* The first medium whose type matches type in the bits of mask (all bits if 0). */
IONetworkMedium *
IONetworkMedium::getMediumWithType(const OSDictionary *dict, IOMediumType type,
    IOMediumType mask)
{
	OSCollectionIterator *iter;
	IONetworkMedium *found = NULL;
	const OSSymbol *key;

	if (dict == NULL || (iter = OSCollectionIterator::withCollection(dict)) == NULL)
		return NULL;
	if (mask == 0)
		mask = ~0U;
	while ((key = OSDynamicCast(OSSymbol, iter->getNextObject())) != NULL) {
		IONetworkMedium *m = OSDynamicCast(IONetworkMedium, dict->getObject(key));

		if (m != NULL && (m->getType() & mask) == (type & mask)) {
			found = m;
			break;
		}
	}
	iter->release();
	return found;
}

IONetworkMedium *
IONetworkMedium::getMediumWithIndex(const OSDictionary *dict, UInt32 index,
    UInt32 mask)
{
	OSCollectionIterator *iter;
	IONetworkMedium *found = NULL;
	const OSSymbol *key;

	if (dict == NULL || (iter = OSCollectionIterator::withCollection(dict)) == NULL)
		return NULL;
	if (mask == 0)
		mask = ~0U;
	while ((key = OSDynamicCast(OSSymbol, iter->getNextObject())) != NULL) {
		IONetworkMedium *m = OSDynamicCast(IONetworkMedium, dict->getObject(key));

		if (m != NULL && (m->getIndex() & mask) == (index & mask)) {
			found = m;
			break;
		}
	}
	iter->release();
	return found;
}
