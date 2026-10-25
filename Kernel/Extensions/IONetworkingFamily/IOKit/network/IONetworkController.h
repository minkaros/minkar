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
 * IONetworkController: the base class of network drivers. A driver
 * subclasses it (usually through IOEthernetController), sets up the
 * hardware in start(), and calls attachInterface() to publish a network
 * interface; the interface becomes a BSD interface (en0, ...) when it is
 * registered. The stack then calls enable() when the interface goes up,
 * disable() when it goes down, and outputPacket() for each packet to send.
 * Received packets go to IONetworkInterface::inputPacket().
 *
 * enable(), disable(), the packet filter and medium calls run inside the
 * controller's command gate, on its work loop.
 */

#ifndef _IONETWORKCONTROLLER_H
#define _IONETWORKCONTROLLER_H

#ifdef KERNEL
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#endif /* KERNEL */
#include <IOKit/network/IONetworkMedium.h>
#ifdef KERNEL
#include <IOKit/network/IOOutputQueue.h>
#include <sys/kpi_mbuf.h>
#endif /* KERNEL */

#ifdef KERNEL
class IONetworkInterface;
#endif /* KERNEL */

/* Property keys. */
#define kIONetworkControllerClass	"IONetworkController"
#define kIOVendor			"IOVendor"
#define kIOModel			"IOModel"
#define kIORevision			"IORevision"
#define kIOFeatures			"IONetworkFeatures"
#define kIOMediumDictionary		"IOMediumDictionary"
#define kIODefaultMedium		"IODefaultMedium"
#define kIOSelectedMedium		"IOSelectedMedium"
#define kIOActiveMedium			"IOActiveMedium"
#define kIOLinkSpeed			"IOLinkSpeed"
#define kIOLinkStatus			"IOLinkStatus"
#define kIOLinkData			"IOLinkData"
#define kIOPacketFilters		"IOPacketFilters"
#define kIOMACAddress			"IOMACAddress"
#define kIOMaxPacketSize		"IOMaxPacketSize"
#define kIOMinPacketSize		"IOMinPacketSize"

/* The packet filter group every controller has. */
#define kIONetworkFilterGroup		"IONetworkFilterGroup"
#ifdef KERNEL
extern const OSSymbol *gIONetworkFilterGroup;
#endif /* KERNEL */

/* Filters in kIONetworkFilterGroup. */
enum {
	kIOPacketFilterUnicast		= 0x00000001,
	kIOPacketFilterBroadcast	= 0x00000002,
	kIOPacketFilterMulticast	= 0x00000010,
	kIOPacketFilterMulticastAll	= 0x00000020,
	kIOPacketFilterPromiscuous	= 0x00000100,
	kIOPacketFilterPromiscuousAll	= 0x00000200,
};

/* getFeatures() bits. */
enum {
	kIONetworkFeatureNoBSDWait		= 0x00000001,
	kIONetworkFeatureHardwareVlan		= 0x00000002,
	kIONetworkFeatureSoftwareVlan		= 0x00000004,
	kIONetworkFeatureMultiPages		= 0x00000008,
	kIONetworkFeatureTSOIPv4		= 0x00000010,
	kIONetworkFeatureTSOIPv6		= 0x00000020,
};

/* Checksum families and bits for getChecksumSupport(). */
enum {
	kChecksumFamilyTCPIP		= 0x00000001,
};
enum {
	kChecksumIP			= 0x0001,
	kChecksumTCP			= 0x0002,
	kChecksumUDP			= 0x0004,
	kChecksumTCPIPv6		= 0x0020,
	kChecksumUDPIPv6		= 0x0040,
	kChecksumTCPNoPseudoHeader	= 0x0100,
	kChecksumUDPNoPseudoHeader	= 0x0200,
	kChecksumTCPSum16		= 0x1000,
};

#ifdef KERNEL
class IONetworkController : public IOService
{
	OSDeclareAbstractStructors(IONetworkController);

public:
	typedef void (*Action)(void *target, void *param0, void *param1,
	    void *param2, void *param3);

protected:
	IOWorkLoop *		_workLoop;
	IOCommandGate *		_commandGate;
	IOOutputQueue *		_outputQueue;
	IONetworkInterface *	_interface;	/* the attached interface */
	OSDictionary *		_mediumDict;
	const IONetworkMedium *	_selectedMedium;
	UInt32			_linkStatus;
	UInt64			_linkSpeed;
	UInt32			_filters;	/* enabled kIONetworkFilterGroup filters */
	bool			_enabled;
	bool			_defaultPM;	/* the family handles power */
	bool			_enabledBeforeSleep;

	virtual void free() APPLE_KEXT_OVERRIDE;

	/* Creates the work loop getWorkLoop() returns; the default is a new one. */
	virtual bool createWorkLoop();

	/* The output queue for outputPacket(); NULL means no queue. */
	virtual IOOutputQueue *createOutputQueue();

	/* Interfaces. */
	virtual IONetworkInterface *createInterface() = 0;
	virtual bool configureInterface(IONetworkInterface *interface);

	bool attachInterface(IONetworkInterface **interface, bool doRegister = true);
	void detachInterface(IONetworkInterface *interface, bool sync = false);

	/* Media and link status. */
	bool publishMediumDictionary(const OSDictionary *mediumDict);
	bool setSelectedMedium(const IONetworkMedium *medium);
	bool setLinkStatus(UInt32 status, const IONetworkMedium *activeMedium = NULL,
	    UInt64 speed = 0, OSData *data = NULL);

	/* Called by the controller when its hardware address changes. */
	virtual bool publishProperties();
	void publishSupportedFilters();
	void startDefaultPowerManagement(IOService *provider);

public:
	/* Power changes, inside the command gate (family power management). */
	void powerChange(bool on);

protected:

public:
	static void initialize();

	virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

	virtual IOWorkLoop *getWorkLoop() const APPLE_KEXT_OVERRIDE;
	IOCommandGate *getCommandGate() const;
	virtual IOOutputQueue *getOutputQueue() const;

	/* Runs action(target, param0..3) inside the command gate. */
	IOReturn executeCommand(OSObject *client, Action action, void *target,
	    void *param0 = NULL, void *param1 = NULL, void *param2 = NULL,
	    void *param3 = NULL);

	/* Called by the interface when it goes up and down. */
	virtual IOReturn enable(IONetworkInterface *interface);
	virtual IOReturn disable(IONetworkInterface *interface);

	/* Sends one packet; returns a kIOReturnOutput* code. */
	virtual UInt32 outputPacket(mbuf_t packet, void *param);

	virtual IOReturn getHardwareAddress(void *addr, UInt32 *inOutAddrBytes) = 0;
	virtual IOReturn setHardwareAddress(const void *addr, UInt32 addrBytes) = 0;

	virtual IOReturn getPacketFilters(const OSSymbol *group,
	    UInt32 *filters) const;
	virtual IOReturn enablePacketFilter(const OSSymbol *group, UInt32 aFilter,
	    UInt32 enabledFilters, IOOptionBits options = 0);
	virtual IOReturn disablePacketFilter(const OSSymbol *group, UInt32 aFilter,
	    UInt32 enabledFilters, IOOptionBits options = 0);

	virtual IOReturn getMaxPacketSize(UInt32 *maxSize) const = 0;
	virtual IOReturn getMinPacketSize(UInt32 *minSize) const = 0;
	virtual IOReturn selectMedium(const IONetworkMedium *medium);
	virtual IOReturn getChecksumSupport(UInt32 *checksumMask,
	    UInt32 checksumFamily, bool isOutput);
	virtual UInt32 getFeatures() const;
	virtual const OSString *newVendorString() const;
	virtual const OSString *newModelString() const;
	virtual const OSString *newRevisionString() const;

	const OSDictionary *getMediumDictionary() const;
	OSDictionary *copyMediumDictionary() const;
	const IONetworkMedium *getSelectedMedium() const;
	UInt32 getLinkStatus() const;
	UInt64 getLinkSpeed() const;

	/* Packets: allocatePacket() returns a packet of size bytes, or NULL. */
	mbuf_t allocatePacket(UInt32 size);
	void freePacket(mbuf_t packet, IOOptionBits options = 0);
	mbuf_t copyPacket(const mbuf_t packet, UInt32 size = 0);

	/* Minkar: the interface's packet filters changed (IFF_PROMISC, multicast). */
	virtual IOReturn updateFilters(UInt32 filters);

	/*
	 * Checksum offload, for drivers: what the stack asks to be computed on
	 * a packet to send, and what the hardware verified on a received one.
	 * Masks are kChecksum* bits.
	 */
	virtual void getChecksumDemand(const mbuf_t packet, UInt32 checksumFamily,
	    UInt32 *demandMask, void *param0 = NULL, void *param1 = NULL);
	virtual bool setChecksumResult(mbuf_t packet, UInt32 checksumFamily,
	    UInt32 resultMask, UInt32 validMask, UInt32 param0 = 0, UInt32 param1 = 0);

	/*
	 * Power: a driver that manages its own power states returns
	 * kIOReturnSuccess from registerWithPolicyMaker(); otherwise the family
	 * registers two states and turns the controller off (disable()) and on
	 * (enable()) around system sleep.
	 */
	virtual IOReturn registerWithPolicyMaker(IOService *policyMaker);
	virtual IOReturn setPowerState(unsigned long powerStateOrdinal,
	    IOService *policyMaker) APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IONetworkController, 0);
	OSMetaClassDeclareReservedUnused(IONetworkController, 1);
	OSMetaClassDeclareReservedUnused(IONetworkController, 2);
	OSMetaClassDeclareReservedUnused(IONetworkController, 3);
	OSMetaClassDeclareReservedUnused(IONetworkController, 4);
	OSMetaClassDeclareReservedUnused(IONetworkController, 5);
	OSMetaClassDeclareReservedUnused(IONetworkController, 6);
	OSMetaClassDeclareReservedUnused(IONetworkController, 7);
};
#endif /* KERNEL */

#endif /* !_IONETWORKCONTROLLER_H */
