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
 * Output queues: hold the packets the network stack sends until the
 * controller takes them. A controller's outputPacket() returns
 * kIOReturnOutputStall when its transmit ring is full; the queue keeps
 * that packet and stops until the controller calls service().
 *
 * IOBasicOutputQueue calls outputPacket() from the thread that queued the
 * packet (or from a thread call after service(kServiceAsync)), one call
 * at a time. IOGatedOutputQueue makes the same calls inside the command
 * gate of a work loop, so they never run at the same time as the
 * controller's interrupt handler.
 */

#ifndef _IOOUTPUTQUEUE_H
#define _IOOUTPUTQUEUE_H

#ifdef KERNEL
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOLocks.h>
#include <sys/kpi_mbuf.h>
#include <kern/thread_call.h>
#endif /* KERNEL */

/* outputPacket() results. */
enum {
	kIOReturnOutputSuccess	= 0x00,		/* the controller took the packet */
	kIOReturnOutputStall	= 0x01,		/* retry the packet after service() */
	kIOReturnOutputDropped	= 0x02,		/* the controller freed the packet */
	kIOOutputStatusMask	= 0xff,
	kIOOutputCommandMask	= 0xff00,
	kIOOutputCommandNone	= 0x0000,
	kIOOutputCommandStall	= 0x0100,
};

/* service() options. */
enum {
	kServiceAsync		= 0x1,
};

/* The controller's transmit function. */
#ifdef KERNEL
typedef UInt32 (OSObject::*IOOutputAction)(mbuf_t packet, void *param);
#endif /* KERNEL */

#ifdef KERNEL
class IOOutputQueue : public OSObject
{
	OSDeclareAbstractStructors(IOOutputQueue);

public:
	virtual bool start() = 0;
	virtual bool stop() = 0;
	virtual bool service(IOOptionBits options = 0) = 0;
	virtual UInt32 flush() = 0;
	virtual bool setCapacity(UInt32 capacity) = 0;
	virtual UInt32 getCapacity() const = 0;
	virtual UInt32 getSize() const = 0;
	virtual UInt32 getDropCount() = 0;
	virtual UInt32 getOutputCount() = 0;
	virtual UInt32 getRetryCount() = 0;
	virtual UInt32 getStallCount() = 0;

	/* Queues a packet (or a chain linked with mbuf_setnextpkt). */
	virtual UInt32 enqueue(mbuf_t packet, void *param) = 0;

	OSMetaClassDeclareReservedUnused(IOOutputQueue, 0);
	OSMetaClassDeclareReservedUnused(IOOutputQueue, 1);
	OSMetaClassDeclareReservedUnused(IOOutputQueue, 2);
	OSMetaClassDeclareReservedUnused(IOOutputQueue, 3);
};
#endif /* KERNEL */

#ifdef KERNEL
class IOBasicOutputQueue : public IOOutputQueue
{
	OSDeclareDefaultStructors(IOBasicOutputQueue);

protected:
	OSObject *		_target;
	IOOutputAction		_action;
	IOLock *		_lock;
	thread_call_t		_callout;
	mbuf_t			_head;
	mbuf_t			_tail;
	UInt32			_size;
	UInt32			_capacity;
	bool			_started;
	bool			_stalled;
	bool			_running;	/* a thread is sending */
	UInt32			_dropCount;
	UInt32			_outputCount;
	UInt32			_retryCount;
	UInt32			_stallCount;

	virtual void free() APPLE_KEXT_OVERRIDE;

	/* Sends queued packets until the queue is empty or stalls. */
	virtual void dequeueLoop();
	static void serviceCallout(thread_call_param_t queue, thread_call_param_t);

public:
	enum { kDefaultCapacity = 100 };

	static IOBasicOutputQueue *withTarget(IOService *target,
	    UInt32 capacity = 0);
	static IOBasicOutputQueue *withTarget(OSObject *target,
	    IOOutputAction action, UInt32 capacity = 0);

	virtual bool init(OSObject *target, IOOutputAction action,
	    UInt32 capacity = 0);

	virtual bool start() APPLE_KEXT_OVERRIDE;
	virtual bool stop() APPLE_KEXT_OVERRIDE;
	virtual bool service(IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
	virtual UInt32 flush() APPLE_KEXT_OVERRIDE;
	virtual bool setCapacity(UInt32 capacity) APPLE_KEXT_OVERRIDE;
	virtual UInt32 getCapacity() const APPLE_KEXT_OVERRIDE;
	virtual UInt32 getSize() const APPLE_KEXT_OVERRIDE;
	virtual UInt32 getDropCount() APPLE_KEXT_OVERRIDE;
	virtual UInt32 getOutputCount() APPLE_KEXT_OVERRIDE;
	virtual UInt32 getRetryCount() APPLE_KEXT_OVERRIDE;
	virtual UInt32 getStallCount() APPLE_KEXT_OVERRIDE;
	virtual UInt32 enqueue(mbuf_t packet, void *param) APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IOBasicOutputQueue, 0);
	OSMetaClassDeclareReservedUnused(IOBasicOutputQueue, 1);
	OSMetaClassDeclareReservedUnused(IOBasicOutputQueue, 2);
	OSMetaClassDeclareReservedUnused(IOBasicOutputQueue, 3);
};
#endif /* KERNEL */

#ifdef KERNEL
class IOGatedOutputQueue : public IOBasicOutputQueue
{
	OSDeclareDefaultStructors(IOGatedOutputQueue);

protected:
	IOCommandGate *		_gate;

	virtual void free() APPLE_KEXT_OVERRIDE;
	virtual void dequeueLoop() APPLE_KEXT_OVERRIDE;
	static IOReturn gatedDequeue(OSObject *owner, void *, void *, void *, void *);

public:
	static IOGatedOutputQueue *withTarget(IOService *target,
	    IOWorkLoop *workloop, UInt32 capacity = 0);
	static IOGatedOutputQueue *withTarget(OSObject *target,
	    IOOutputAction action, IOWorkLoop *workloop, UInt32 capacity = 0);

	virtual bool init(OSObject *target, IOOutputAction action,
	    IOWorkLoop *workloop, UInt32 capacity = 0);

	OSMetaClassDeclareReservedUnused(IOGatedOutputQueue, 0);
	OSMetaClassDeclareReservedUnused(IOGatedOutputQueue, 1);
};
#endif /* KERNEL */

#endif /* !_IOOUTPUTQUEUE_H */
