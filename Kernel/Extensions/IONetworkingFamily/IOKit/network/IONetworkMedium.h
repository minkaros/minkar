/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * IONetworkMedium: one medium a network controller supports (type, speed,
 * flags), and the link status values. Medium types are the BSD ifmedia
 * words (<net/if_media.h>), so they pass to ifconfig unchanged.
 */

#ifndef _IONETWORKMEDIUM_H
#define _IONETWORKMEDIUM_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOReturn.h>
#include <net/if_media.h>
#ifdef KERNEL
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSDictionary.h>
#endif

typedef UInt32 IOMediumType;

/* Medium types. */
enum {
	kIOMediumEthernet		= IFM_ETHER,
	kIOMediumEthernetAuto		= IFM_ETHER | IFM_AUTO,
	kIOMediumEthernetManual		= IFM_ETHER | IFM_MANUAL,
	kIOMediumEthernetNone		= IFM_ETHER | IFM_NONE,
	kIOMediumEthernet10BaseT	= IFM_ETHER | IFM_10_T,
	kIOMediumEthernet100BaseTX	= IFM_ETHER | IFM_100_TX,
	kIOMediumEthernet1000BaseT	= IFM_ETHER | IFM_1000_T,
};

/* Medium options. */
enum {
	kIOMediumOptionFullDuplex	= IFM_FDX,
	kIOMediumOptionHalfDuplex	= IFM_HDX,
	kIOMediumOptionFlowControl	= IFM_FLOW,
	kIOMediumOptionLoopback		= IFM_LOOP,
};

/* Masks for the fields of an IOMediumType. */
enum {
	kIOMediumNetworkTypeMask	= IFM_NMASK,
	kIOMediumSubTypeMask		= IFM_TMASK,
	kIOMediumOptionsMask		= IFM_GMASK,
	kIOMediumCommonOptionsMask	= IFM_GMASK,
	kIOMediumInstanceMask		= IFM_IMASK,
};

/* Link status bits, given to IONetworkController::setLinkStatus(). */
enum {
	kIONetworkLinkValid		= IFM_AVALID,
	kIONetworkLinkActive		= IFM_ACTIVE,
	kIONetworkLinkNoNetworkChange	= 0x00000004,
};

/* Keys of a serialized medium and of the controller's link properties. */
#define kIOMediumType		"Type"
#define kIOMediumSpeed		"Speed"
#define kIOMediumFlags		"Flags"
#define kIOMediumIndex		"Index"

/* The class is the kernel's; user space sees the constants. */
#ifdef KERNEL
class IONetworkMedium : public OSObject
{
	OSDeclareDefaultStructors(IONetworkMedium);

protected:
	IOMediumType		_type;
	UInt32			_flags;
	UInt64			_speed;
	UInt32			_index;
	const OSSymbol *	_name;

	virtual void free() APPLE_KEXT_OVERRIDE;

public:
	/* A new medium; with no name, the name is the type in hex. */
	static IONetworkMedium *medium(IOMediumType type, UInt64 speed,
	    UInt32 flags = 0, UInt32 index = 0, const char *name = NULL);

	virtual bool init(IOMediumType type, UInt64 speed, UInt32 flags = 0,
	    UInt32 index = 0, const char *name = NULL);

	IOMediumType	getType() const;
	UInt64		getSpeed() const;
	UInt32		getFlags() const;
	UInt32		getIndex() const;
	const OSSymbol *getName() const;
	const OSSymbol *getKey() const;

	bool isEqualTo(const IONetworkMedium *medium) const;
	virtual bool isEqualTo(const OSMetaClassBase *obj) const APPLE_KEXT_OVERRIDE;
	virtual bool serialize(OSSerialize *s) const APPLE_KEXT_OVERRIDE;

	/* The name an IOMediumType gets when none is given. */
	static const OSSymbol *nameForType(IOMediumType type);

	/* Medium dictionaries: name -> IONetworkMedium. */
	static IOReturn addMedium(OSDictionary *dict, const IONetworkMedium *medium);
	static IOReturn removeMedium(OSDictionary *dict, const IONetworkMedium *medium);
	static IONetworkMedium *getMediumWithType(const OSDictionary *dict,
	    IOMediumType type, IOMediumType mask = 0);
	static IONetworkMedium *getMediumWithIndex(const OSDictionary *dict,
	    UInt32 index, UInt32 mask = 0);

	OSMetaClassDeclareReservedUnused(IONetworkMedium, 0);
	OSMetaClassDeclareReservedUnused(IONetworkMedium, 1);
	OSMetaClassDeclareReservedUnused(IONetworkMedium, 2);
	OSMetaClassDeclareReservedUnused(IONetworkMedium, 3);
};

#endif /* KERNEL */

#endif /* !_IONETWORKMEDIUM_H */
