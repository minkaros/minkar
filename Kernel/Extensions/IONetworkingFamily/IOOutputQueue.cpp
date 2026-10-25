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

#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/IOLib.h>

OSDefineMetaClassAndAbstractStructors(IOOutputQueue, OSObject);
OSMetaClassDefineReservedUnused(IOOutputQueue, 0);
OSMetaClassDefineReservedUnused(IOOutputQueue, 1);
OSMetaClassDefineReservedUnused(IOOutputQueue, 2);
OSMetaClassDefineReservedUnused(IOOutputQueue, 3);

#define super IOOutputQueue
OSDefineMetaClassAndStructors(IOBasicOutputQueue, IOOutputQueue);
OSMetaClassDefineReservedUnused(IOBasicOutputQueue, 0);
OSMetaClassDefineReservedUnused(IOBasicOutputQueue, 1);
OSMetaClassDefineReservedUnused(IOBasicOutputQueue, 2);
OSMetaClassDefineReservedUnused(IOBasicOutputQueue, 3);

IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(IOService *target, UInt32 capacity)
{
	IONetworkController *ctl = OSDynamicCast(IONetworkController, target);

	if (ctl == NULL)
		return NULL;
	return withTarget(ctl, (IOOutputAction)&IONetworkController::outputPacket,
	    capacity);
}

IOBasicOutputQueue *
IOBasicOutputQueue::withTarget(OSObject *target, IOOutputAction action,
    UInt32 capacity)
{
	IOBasicOutputQueue *q = new IOBasicOutputQueue;

	if (q != NULL && !q->init(target, action, capacity)) {
		q->release();
		q = NULL;
	}
	return q;
}

bool
IOBasicOutputQueue::init(OSObject *target, IOOutputAction action,
    UInt32 capacity)
{
	if (!super::init() || target == NULL || action == NULL)
		return false;
	_target = target;	/* the controller owns the queue: no retain */
	_action = action;
	_capacity = capacity != 0 ? capacity : (UInt32)kDefaultCapacity;
	_lock = IOLockAlloc();
	_callout = thread_call_allocate(serviceCallout, this);
	return _lock != NULL && _callout != NULL;
}

void
IOBasicOutputQueue::free()
{
	if (_callout != NULL) {
		thread_call_cancel(_callout);
		thread_call_free(_callout);
		_callout = NULL;
	}
	if (_lock != NULL) {
		flush();
		IOLockFree(_lock);
		_lock = NULL;
	}
	super::free();
}

bool
IOBasicOutputQueue::start()
{
	IOLockLock(_lock);
	_started = true;
	_stalled = false;
	IOLockUnlock(_lock);
	service(kServiceAsync);
	return true;
}

bool
IOBasicOutputQueue::stop()
{
	bool was;

	IOLockLock(_lock);
	was = _started;
	_started = false;
	IOLockUnlock(_lock);
	return was;
}

/*
 * Takes packets off the head of the queue and gives them to the target,
 * until the queue is empty, stalls or is stopped. Only one thread runs
 * the loop at a time; a packet queued meanwhile is sent by that thread.
 */
void
IOBasicOutputQueue::dequeueLoop()
{
	mbuf_t m;
	UInt32 status;

	IOLockLock(_lock);
	if (_running) {
		IOLockUnlock(_lock);
		return;
	}
	_running = true;
	while (_started && !_stalled && (m = _head) != NULL) {
		_head = mbuf_nextpkt(m);
		if (_head == NULL)
			_tail = NULL;
		mbuf_setnextpkt(m, NULL);
		_size--;
		IOLockUnlock(_lock);

		status = (_target->*_action)(m, NULL);

		IOLockLock(_lock);
		switch (status & kIOOutputStatusMask) {
		case kIOReturnOutputStall:
			/* Put the packet back at the head and wait for service(). */
			mbuf_setnextpkt(m, _head);
			_head = m;
			if (_tail == NULL)
				_tail = m;
			_size++;
			_stalled = true;
			_stallCount++;
			_retryCount++;
			break;
		case kIOReturnOutputDropped:
			_dropCount++;
			break;
		default:
			_outputCount++;
			break;
		}
	}
	_running = false;
	IOLockUnlock(_lock);
}

void
IOBasicOutputQueue::serviceCallout(thread_call_param_t queue, thread_call_param_t)
{
	IOBasicOutputQueue *q = (IOBasicOutputQueue *)queue;

	q->dequeueLoop();
	q->release();
}

bool
IOBasicOutputQueue::service(IOOptionBits options)
{
	bool wasStalled;

	IOLockLock(_lock);
	wasStalled = _stalled;
	_stalled = false;
	IOLockUnlock(_lock);

	if (options & kServiceAsync) {
		retain();
		if (thread_call_enter(_callout))
			release();	/* already pending */
	} else {
		dequeueLoop();
	}
	return wasStalled;
}

UInt32
IOBasicOutputQueue::flush()
{
	mbuf_t head;
	UInt32 n;

	IOLockLock(_lock);
	head = _head;
	n = _size;
	_head = _tail = NULL;
	_size = 0;
	_dropCount += n;
	IOLockUnlock(_lock);
	if (head != NULL)
		mbuf_freem_list(head);
	return n;
}

bool
IOBasicOutputQueue::setCapacity(UInt32 capacity)
{
	IOLockLock(_lock);
	_capacity = capacity;
	IOLockUnlock(_lock);
	return true;
}

UInt32 IOBasicOutputQueue::getCapacity() const { return _capacity; }
UInt32 IOBasicOutputQueue::getSize() const { return _size; }
UInt32 IOBasicOutputQueue::getDropCount() { return _dropCount; }
UInt32 IOBasicOutputQueue::getOutputCount() { return _outputCount; }
UInt32 IOBasicOutputQueue::getRetryCount() { return _retryCount; }
UInt32 IOBasicOutputQueue::getStallCount() { return _stallCount; }

UInt32
IOBasicOutputQueue::enqueue(mbuf_t packet, void *)
{
	mbuf_t m, next, dropped = NULL;
	UInt32 drops = 0;

	IOLockLock(_lock);
	for (m = packet; m != NULL; m = next) {
		next = mbuf_nextpkt(m);
		mbuf_setnextpkt(m, NULL);
		if (_size >= _capacity) {
			mbuf_setnextpkt(m, dropped);
			dropped = m;
			drops++;
			continue;
		}
		if (_tail != NULL)
			mbuf_setnextpkt(_tail, m);
		else
			_head = m;
		_tail = m;
		_size++;
	}
	_dropCount += drops;
	IOLockUnlock(_lock);
	if (dropped != NULL)
		mbuf_freem_list(dropped);

	dequeueLoop();
	return drops;
}

#undef super
#define super IOBasicOutputQueue
OSDefineMetaClassAndStructors(IOGatedOutputQueue, IOBasicOutputQueue);
OSMetaClassDefineReservedUnused(IOGatedOutputQueue, 0);
OSMetaClassDefineReservedUnused(IOGatedOutputQueue, 1);

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(IOService *target, IOWorkLoop *workloop,
    UInt32 capacity)
{
	IONetworkController *ctl = OSDynamicCast(IONetworkController, target);

	if (ctl == NULL)
		return NULL;
	return withTarget(ctl, (IOOutputAction)&IONetworkController::outputPacket,
	    workloop, capacity);
}

IOGatedOutputQueue *
IOGatedOutputQueue::withTarget(OSObject *target, IOOutputAction action,
    IOWorkLoop *workloop, UInt32 capacity)
{
	IOGatedOutputQueue *q = new IOGatedOutputQueue;

	if (q != NULL && !q->init(target, action, workloop, capacity)) {
		q->release();
		q = NULL;
	}
	return q;
}

bool
IOGatedOutputQueue::init(OSObject *target, IOOutputAction action,
    IOWorkLoop *workloop, UInt32 capacity)
{
	if (workloop == NULL || !super::init(target, action, capacity))
		return false;
	_gate = IOCommandGate::commandGate(this);
	if (_gate == NULL || workloop->addEventSource(_gate) != kIOReturnSuccess)
		return false;
	return true;
}

void
IOGatedOutputQueue::free()
{
	if (_gate != NULL) {
		IOWorkLoop *wl = _gate->getWorkLoop();

		if (wl != NULL)
			wl->removeEventSource(_gate);
		_gate->release();
		_gate = NULL;
	}
	super::free();
}

IOReturn
IOGatedOutputQueue::gatedDequeue(OSObject *owner, void *, void *, void *, void *)
{
	((IOGatedOutputQueue *)owner)->IOBasicOutputQueue::dequeueLoop();
	return kIOReturnSuccess;
}

/* The same loop, inside the work loop's gate. */
void
IOGatedOutputQueue::dequeueLoop()
{
	_gate->runAction(gatedDequeue);
}
