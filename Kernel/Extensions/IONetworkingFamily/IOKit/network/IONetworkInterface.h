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
 * IONetworkInterface: the link between a network controller and the BSD
 * network stack. Registering the interface creates and attaches its ifnet
 * (named prefix + unit, e.g. en0); packets the stack sends go to the
 * controller's output queue, and the controller hands received packets to
 * inputPacket().
 */

#ifndef _IONETWORKINTERFACE_H
#define _IONETWORKINTERFACE_H

#ifdef KERNEL
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <sys/kpi_mbuf.h>
#include <net/kpi_interface.h>
#endif /* KERNEL */
#include <IOKit/network/IONetworkStats.h>

#ifdef KERNEL
class IONetworkController;
class IOOutputQueue;
class IONetworkData;
class IONetworkStack;
#endif /* KERNEL */

/* Property keys. */
#define kIONetworkInterfaceClass	"IONetworkInterface"
#define kIOInterfaceType		"IOInterfaceType"
#define kIOMaxTransferUnit		"IOMaxTransferUnit"
#define kIOMediaAddressLength		"IOMediaAddressLength"
#define kIOMediaHeaderLength		"IOMediaHeaderLength"
#define kIOInterfaceFlags		"IOInterfaceFlags"
#define kIOInterfaceExtraFlags		"IOInterfaceExtraFlags"
#define kIOInterfaceUnit		"IOInterfaceUnit"
#define kIOInterfaceState		"IOInterfaceState"
#define kIOInterfaceNamePrefix		"IOInterfaceNamePrefix"
#define kIOPrimaryInterface		"IOPrimaryInterface"
#define kIOBuiltin			"IOBuiltin"
#define kIOLocation			"IOLocation"

/* kIOInterfaceState bits. */
enum {
	kIONetworkInterfaceRegisteredState	= 0x1,
	kIONetworkInterfaceOpenedByClientState	= 0x2,
	kIONetworkInterfaceDisabledState	= 0x4,
};

/* inputPacket() options. */
enum {
	kInputOptionQueuePacket	= 0x1,	/* queue until flushInputQueue() */
};

#ifdef KERNEL
class IONetworkInterface : public IOService
{
	OSDeclareAbstractStructors(IONetworkInterface);

	friend class IONetworkController;
	friend class IONetworkStack;

protected:
	IONetworkController *	_controller;
	IOOutputQueue *		_outputQueue;
	ifnet_t			_ifp;
	IOLock *		_lock;
	UInt32			_unit;
	UInt32			_mtu;
	UInt32			_flags;		/* ifnet flags before the BSD attach */
	UInt8			_type;		/* IFT_* */
	UInt8			_headerLength;
	UInt8			_addressLength;
	bool			_attached;	/* the ifnet is attached */
	bool			_enabled;	/* the controller is enabled */
	UInt32			_filters;	/* kIONetworkFilterGroup filters */
	mbuf_t			_inputHead;
	mbuf_t			_inputTail;
	UInt32			_inputCount;
	UInt32			_inputBytes;
	OSArray *		_dataList;	/* IONetworkData by handle */
	OSDictionary *		_dataDict;	/* IONetworkData by key */
	IONetworkStats *	_stats;		/* kIONetworkStatsKey's buffer */
	UInt32			_lastInputErrors;
	UInt32			_lastOutputErrors;
	UInt32			_lastCollisions;

	virtual void free() APPLE_KEXT_OVERRIDE;

	/* Fills in the ifnet parameters; subclasses set demux, framer, etc. */
	virtual bool initIfnetParams(struct ifnet_init_params *params);

	/* The link-level address the ifnet is attached with, or NULL. */
	virtual const struct sockaddr_dl *copyLinkAddress(struct sockaddr_dl *buf);

	/* Attaches the ifnet; called by registerService(). */
	virtual bool attachToDataLinkLayer(IOOptionBits options, void *parameter);
	virtual void detachFromDataLinkLayer(IOOptionBits options, void *parameter);

	/* ioctl requests from the stack. */
	virtual SInt32 performCommand(IONetworkController *controller,
	    unsigned long cmd, void *arg0, void *arg1);
	virtual SInt32 syncFlags();
	virtual SInt32 setFilters(UInt32 filters);

	/* The BPF data link type (DLT_*) for this interface. */
	virtual UInt32 getDataLinkType() const;

	/* Called by the controller when the link status changes. */
	virtual void linkStatusChanged(UInt32 status, UInt64 speed);

	/* Gives the stack the controller's checksum, TSO and VLAN offloads. */
	virtual void configureOffloads();
	/* Pushes the driver's error counts to the ifnet. */
	void syncErrorStats();
	OSDictionary *_requiredFilters();
	void publishRequiredFilters();
	static IOReturn outputQueueStatsAction(void *target, void *param, IONetworkData *data,
	    UInt32 accessType, void *buffer, UInt32 *bufferSize, UInt32 offset);

	static errno_t ifOutput(ifnet_t ifp, mbuf_t m);
	static errno_t ifIoctl(ifnet_t ifp, unsigned long cmd, void *data);
	static void ifDetached(ifnet_t ifp);

public:
	virtual bool init(IONetworkController *controller);

	IONetworkController *getController() const;
	ifnet_t getIfnet() const;
	virtual const char *getNamePrefix() const = 0;
	UInt32 getUnitNumber() const;
	bool isPrimaryInterface() const;

	UInt8 getInterfaceType() const;
	bool setInterfaceType(UInt8 type);
	UInt32 getMaxTransferUnit() const;
	bool setMaxTransferUnit(UInt32 mtu);
	UInt32 getMediaAddressLength() const;
	bool setMediaAddressLength(UInt32 length);
	UInt32 getMediaHeaderLength() const;
	bool setMediaHeaderLength(UInt32 length);
	UInt32 getFlags() const;
	bool setFlags(UInt32 flags, UInt32 clear = 0);

	/*
	 * Hands a received packet (link-level header included) to the stack.
	 * With kInputOptionQueuePacket the packet waits for flushInputQueue(),
	 * so a driver can pass several packets to the stack at once. Returns
	 * the number of packets passed to the stack.
	 */
	virtual UInt32 inputPacket(mbuf_t packet, UInt32 length = 0,
	    IOOptionBits options = 0, void *param = NULL);
	virtual UInt32 flushInputQueue();
	virtual UInt32 clearInputQueue();

	/* Publishes the interface; IONetworkStack names it (attaches the ifnet). */
	virtual void registerService(IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
	bool attachToDataLinkLayerWithUnit(UInt32 unit);

	/* Network data (statistics...), also published as "IONetworkData". */
	virtual bool addNetworkData(IONetworkData *data);
	virtual bool removeNetworkData(const OSSymbol *key);
	IONetworkData *getNetworkData(const OSSymbol *key) const;
	IONetworkData *getNetworkData(const char *key) const;
	bool getNetworkDataHandle(const char *key, UInt32 *handle) const;
	IONetworkData *getNetworkDataForHandle(UInt32 handle) const;
	IONetworkStats *getNetworkStats() const;

	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IONetworkInterface, 0);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 1);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 2);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 3);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 4);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 5);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 6);
	OSMetaClassDeclareReservedUnused(IONetworkInterface, 7);
};
#endif /* KERNEL */

#endif /* !_IONETWORKINTERFACE_H */
