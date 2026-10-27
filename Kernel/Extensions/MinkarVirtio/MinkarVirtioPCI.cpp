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

#include "MinkarVirtioPCI.h"
#include <IOKit/IOLib.h>

#define LOG(fmt, ...) IOLog("MinkarVirtio: " fmt "\n", ##__VA_ARGS__)

static inline void
barrier()
{
	__sync_synchronize();
}

/* Registers at a byte offset of a mapped area (naturally aligned there). */
static inline volatile uint16_t *
reg16(volatile uint8_t *base, uint32_t off)
{
	return (volatile uint16_t *)(uintptr_t)(base + off);
}

static inline volatile uint32_t *
reg32(volatile uint8_t *base, uint32_t off)
{
	return (volatile uint32_t *)(uintptr_t)(base + off);
}

IOBufferMemoryDescriptor *
MinkarVirtioAllocDMA(IOByteCount size)
{
	IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
	    kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut, size,
	    0x00000000FFFFF000ULL);

	if (mem != NULL)
		bzero(mem->getBytesNoCopy(), size);
	return mem;
}

int
MinkarVirtioInterruptIndex(IOPCIDevice *pci)
{
	for (int i = 0; i < 8; i++) {
		int type;

		if (pci->getInterruptType(i, &type) != kIOReturnSuccess)
			break;
		if ((type & kIOInterruptTypePCIMessaged) == 0)
			return i;
	}
	return 0;
}

/* ---- Split virtqueue --------------------------------------------------- */

OSDefineMetaClassAndStructors(MinkarVirtQueue, OSObject);

MinkarVirtQueue *
MinkarVirtQueue::withSize(UInt16 index, UInt16 size)
{
	MinkarVirtQueue *q = new MinkarVirtQueue;

	if (q != NULL && !q->initWithSize(index, size))
		OSSafeReleaseNULL(q);
	return q;
}

bool
MinkarVirtQueue::initWithSize(UInt16 index, UInt16 size)
{
	if (!OSObject::init() || size == 0 || (size & (size - 1)) != 0)
		return false;
	fIndex = index;
	fSize = size;
	/* Descriptors, then the driver ring, then the device ring on its own page. */
	fAvailOffset = (IOByteCount)size * sizeof(struct virtq_desc);
	fUsedOffset = round_page(fAvailOffset + 6 + 2 * (IOByteCount)size);
	fMem = MinkarVirtioAllocDMA(fUsedOffset + round_page(6 + 8 * (IOByteCount)size));
	fCookies = (void **)IOMalloc(sizeof(void *) * size);
	if (fMem == NULL || fCookies == NULL)
		return false;
	bzero(fCookies, sizeof(void *) * size);
	fPhys = fMem->getPhysicalAddress();
	uint8_t *base = (uint8_t *)fMem->getBytesNoCopy();
	fDesc = (volatile struct virtq_desc *)(uintptr_t)base;
	fAvail = (volatile uint16_t *)(uintptr_t)(base + fAvailOffset);
	fUsedHdr = (volatile uint16_t *)(uintptr_t)(base + fUsedOffset);
	fUsedRing = (volatile struct virtq_used_elem *)(uintptr_t)(base + fUsedOffset + 4);
	/* All descriptors on the free list, chained through `next`. */
	for (UInt16 i = 0; i < size; i++)
		fDesc[i].next = (uint16_t)(i + 1 < size ? i + 1 : 0);
	fFreeHead = 0;
	fNumFree = size;
	return true;
}

void
MinkarVirtQueue::free()
{
	OSSafeReleaseNULL(fMem);
	if (fCookies != NULL)
		IOFree(fCookies, sizeof(void *) * fSize);
	OSObject::free();
}

bool
MinkarVirtQueue::addChain(const uint64_t *addrs, const uint32_t *lens,
    unsigned nOut, unsigned nIn, void *cookie)
{
	unsigned n = nOut + nIn;

	if (n == 0 || n > fNumFree || cookie == NULL)
		return false;
	UInt16 head = fFreeHead, d = head, last = head;
	for (unsigned i = 0; i < n; i++) {
		fDesc[d].addr = addrs[i];
		fDesc[d].len = lens[i];
		fDesc[d].flags = (uint16_t)((i >= nOut ? VIRTQ_DESC_F_WRITE : 0) |
		    (i + 1 < n ? VIRTQ_DESC_F_NEXT : 0));
		last = d;
		d = fDesc[d].next;
	}
	fFreeHead = d;
	fNumFree -= (UInt16)n;
	(void)last;
	fCookies[head] = cookie;

	fAvail[2 + (fAvailIdx % fSize)] = head;
	barrier();		/* the ring entry before the index */
	fAvailIdx++;
	fAvail[1] = fAvailIdx;
	barrier();		/* the index before the notification */
	return true;
}

bool
MinkarVirtQueue::hasUsed() const
{
	return fUsedHdr[1] != fLastUsed;
}

void *
MinkarVirtQueue::getUsed(uint32_t *len)
{
	if (fUsedHdr[1] == fLastUsed)
		return NULL;
	barrier();		/* the index before the entry */
	volatile struct virtq_used_elem *e = &fUsedRing[fLastUsed % fSize];
	UInt16 head = (UInt16)e->id;
	if (len != NULL)
		*len = e->len;
	fLastUsed++;
	if (head >= fSize)
		return NULL;

	void *cookie = fCookies[head];
	fCookies[head] = NULL;
	/* The chain goes back on the free list. */
	UInt16 d = head, count = 1;
	while (fDesc[d].flags & VIRTQ_DESC_F_NEXT) {
		d = fDesc[d].next;
		count++;
	}
	fDesc[d].next = fFreeHead;
	fFreeHead = head;
	fNumFree += count;
	return cookie;
}

/* ---- PCI transport ----------------------------------------------------- */

OSDefineMetaClassAndStructors(MinkarVirtioPCI, OSObject);

MinkarVirtioPCI *
MinkarVirtioPCI::withDevice(IOPCIDevice *pci)
{
	MinkarVirtioPCI *t = new MinkarVirtioPCI;

	if (t != NULL && !t->initWithDevice(pci))
		OSSafeReleaseNULL(t);
	return t;
}

bool
MinkarVirtioPCI::initWithDevice(IOPCIDevice *pci)
{
	if (!OSObject::init() || pci == NULL)
		return false;
	fPCI = pci;
	fPCI->retain();
	fPCI->setMemoryEnable(true);
	fPCI->setBusMasterEnable(true);
	if (!locateCapabilities()) {
		LOG("no virtio 1.x capabilities (a legacy-only device?)");
		return false;
	}
	return true;
}

void
MinkarVirtioPCI::free()
{
	for (unsigned i = 0; i < 6; i++) {
		OSSafeReleaseNULL(fMaps[i]);
		OSSafeReleaseNULL(fDescs[i]);
	}
	OSSafeReleaseNULL(fPCI);
	OSObject::free();
}

/* A memory BAR mapped uncached; the IODeviceMemory if the platform made one. */
volatile uint8_t *
MinkarVirtioPCI::mapBAR(unsigned bar)
{
	if (bar >= 6)
		return NULL;
	if (fBARs[bar] != NULL)
		return fBARs[bar];

	UInt8 reg = (UInt8)(kIOPCIConfigBaseAddress0 + 4 * bar);
	fMaps[bar] = fPCI->mapDeviceMemoryWithRegister(reg, kIOMapInhibitCache);
	if (fMaps[bar] == NULL) {
		uint32_t lo = fPCI->configRead32(reg);
		uint64_t base;
		uint32_t size;

		if ((lo & 1) != 0)
			return NULL;	/* I/O space: not used by the modern interface */
		base = lo & ~0xFULL;
		if ((lo & 0x6) == 0x4 && bar < 5)	/* 64-bit BAR */
			base |= (uint64_t)fPCI->configRead32((UInt8)(reg + 4)) << 32;
		if (base == 0)
			return NULL;
		fPCI->configWrite32(reg, 0xFFFFFFFF);
		size = ~(fPCI->configRead32(reg) & ~0xFU) + 1;
		fPCI->configWrite32(reg, lo);
		fDescs[bar] = IOMemoryDescriptor::withPhysicalAddress(base, size,
		    kIODirectionNone);
		if (fDescs[bar] == NULL)
			return NULL;
		fMaps[bar] = fDescs[bar]->map(kIOMapAnywhere | kIOMapInhibitCache);
		if (fMaps[bar] == NULL)
			return NULL;
	}
	fBARs[bar] = (volatile uint8_t *)fMaps[bar]->getVirtualAddress();
	return fBARs[bar];
}

volatile uint8_t *
MinkarVirtioPCI::areaAt(unsigned bar, uint32_t offset, uint32_t length)
{
	volatile uint8_t *base = mapBAR(bar);

	if (base == NULL || fMaps[bar] == NULL ||
	    (uint64_t)offset + length > fMaps[bar]->getLength())
		return NULL;
	return base + offset;
}

/* The first capability of each type wins, as the specification asks. */
bool
MinkarVirtioPCI::locateCapabilities()
{
	if ((fPCI->configRead16(kIOPCIConfigStatus) & 0x10) == 0)
		return false;	/* no capability list */
	UInt8 ptr = fPCI->configRead8(kIOPCIConfigCapabilitiesPtr) & 0xFC;
	for (int guard = 0; ptr != 0 && guard < 48; guard++) {
		UInt8 id = fPCI->configRead8(ptr);
		UInt8 next = fPCI->configRead8((UInt8)(ptr + 1)) & 0xFC;

		if (id == 0x09) {	/* vendor specific */
			UInt8 type = fPCI->configRead8((UInt8)(ptr + VIRTIO_PCI_CAP_CFG_TYPE));
			UInt8 bar = fPCI->configRead8((UInt8)(ptr + VIRTIO_PCI_CAP_BAR));
			uint32_t off = fPCI->configRead32((UInt8)(ptr + VIRTIO_PCI_CAP_OFFSET));
			uint32_t len = fPCI->configRead32((UInt8)(ptr + VIRTIO_PCI_CAP_LENGTH));

			switch (type) {
			case VIRTIO_PCI_CAP_COMMON_CFG:
				if (fCommon == NULL)
					fCommon = areaAt(bar, off, len);
				break;
			case VIRTIO_PCI_CAP_NOTIFY_CFG:
				if (fNotify == NULL) {
					fNotify = areaAt(bar, off, len);
					fNotifyMultiplier = fPCI->configRead32(
					    (UInt8)(ptr + VIRTIO_PCI_NOTIFY_MULTIPLIER));
				}
				break;
			case VIRTIO_PCI_CAP_ISR_CFG:
				if (fISR == NULL)
					fISR = areaAt(bar, off, len);
				break;
			case VIRTIO_PCI_CAP_DEVICE_CFG:
				if (fDevice == NULL)
					fDevice = areaAt(bar, off, len);
				break;
			}
		}
		ptr = next;
	}
	return fCommon != NULL && fNotify != NULL && fISR != NULL;
}

void
MinkarVirtioPCI::setStatus(uint8_t bits)
{
	fCommon[VIRTIO_COMMON_STATUS] = (uint8_t)(fCommon[VIRTIO_COMMON_STATUS] | bits);
	barrier();
}

void
MinkarVirtioPCI::reset()
{
	fCommon[VIRTIO_COMMON_STATUS] = 0;
	barrier();
	/* The device is reset once the status reads back as 0. */
	for (int i = 0; i < 1000 && fCommon[VIRTIO_COMMON_STATUS] != 0; i++)
		IODelay(100);
}

bool
MinkarVirtioPCI::begin(uint64_t wanted, uint64_t *negotiated)
{
	volatile uint32_t *dfsel = reg32(fCommon, VIRTIO_COMMON_DFSELECT);
	volatile uint32_t *df = reg32(fCommon, VIRTIO_COMMON_DF);
	volatile uint32_t *gfsel = reg32(fCommon, VIRTIO_COMMON_GFSELECT);
	volatile uint32_t *gf = reg32(fCommon, VIRTIO_COMMON_GF);
	uint64_t offered, chosen;

	reset();
	setStatus(VIRTIO_STATUS_ACKNOWLEDGE);
	setStatus(VIRTIO_STATUS_DRIVER);

	*dfsel = 0;
	barrier();
	offered = *df;
	*dfsel = 1;
	barrier();
	offered |= (uint64_t)*df << 32;

	if ((offered & (1ULL << VIRTIO_F_VERSION_1)) == 0) {
		LOG("the device does not offer VIRTIO_F_VERSION_1");
		fail();
		return false;
	}
	chosen = (offered & wanted) | (1ULL << VIRTIO_F_VERSION_1);
	*gfsel = 0;
	barrier();
	*gf = (uint32_t)chosen;
	*gfsel = 1;
	barrier();
	*gf = (uint32_t)(chosen >> 32);
	barrier();

	setStatus(VIRTIO_STATUS_FEATURES_OK);
	if ((fCommon[VIRTIO_COMMON_STATUS] & VIRTIO_STATUS_FEATURES_OK) == 0) {
		LOG("the device refused features 0x%llx", chosen);
		fail();
		return false;
	}
	if (negotiated != NULL)
		*negotiated = chosen;
	return true;
}

UInt16
MinkarVirtioPCI::getQueueMaxSize(UInt16 index)
{
	*reg16(fCommon, VIRTIO_COMMON_Q_SELECT) = index;
	barrier();
	return *reg16(fCommon, VIRTIO_COMMON_Q_SIZE);
}

bool
MinkarVirtioPCI::setupQueue(MinkarVirtQueue *q)
{
	UInt16 index = q->getIndex();

	if (index >= 8 || getQueueMaxSize(index) < q->getSize())
		return false;
	volatile uint8_t *c = fCommon;
	*reg16(c, VIRTIO_COMMON_Q_SIZE) = q->getSize();
	*reg16(c, VIRTIO_COMMON_Q_MSIX) = VIRTIO_MSI_NO_VECTOR;
	*reg32(c, VIRTIO_COMMON_Q_DESC) = (uint32_t)q->getDescPhys();
	*reg32(c, VIRTIO_COMMON_Q_DESC + 4) = (uint32_t)(q->getDescPhys() >> 32);
	*reg32(c, VIRTIO_COMMON_Q_AVAIL) = (uint32_t)q->getAvailPhys();
	*reg32(c, VIRTIO_COMMON_Q_AVAIL + 4) = (uint32_t)(q->getAvailPhys() >> 32);
	*reg32(c, VIRTIO_COMMON_Q_USED) = (uint32_t)q->getUsedPhys();
	*reg32(c, VIRTIO_COMMON_Q_USED + 4) = (uint32_t)(q->getUsedPhys() >> 32);
	UInt16 noff = *reg16(c, VIRTIO_COMMON_Q_NOFF);
	fQueueNotify[index] = reg16(fNotify, (uint32_t)noff * fNotifyMultiplier);
	barrier();
	*reg16(c, VIRTIO_COMMON_Q_ENABLE) = 1;
	barrier();
	return true;
}

void
MinkarVirtioPCI::notify(MinkarVirtQueue *q)
{
	UInt16 index = q->getIndex();

	barrier();
	if (index < 8 && fQueueNotify[index] != NULL)
		*fQueueNotify[index] = index;
}

void
MinkarVirtioPCI::driverOK()
{
	setStatus(VIRTIO_STATUS_DRIVER_OK);
}

void
MinkarVirtioPCI::fail()
{
	setStatus(VIRTIO_STATUS_FAILED);
}

uint8_t
MinkarVirtioPCI::readISR()
{
	return *fISR;
}

uint8_t
MinkarVirtioPCI::configRead8(UInt32 offset)
{
	return fDevice != NULL ? fDevice[offset] : 0;
}

uint16_t
MinkarVirtioPCI::configRead16(UInt32 offset)
{
	return fDevice != NULL ? *reg16(fDevice, offset) : 0;
}

uint32_t
MinkarVirtioPCI::configRead32(UInt32 offset)
{
	return fDevice != NULL ? *reg32(fDevice, offset) : 0;
}

/* Two halves, read again if the device changed its configuration meanwhile. */
uint64_t
MinkarVirtioPCI::configRead64(UInt32 offset)
{
	uint8_t gen;
	uint64_t v;

	if (fDevice == NULL)
		return 0;
	do {
		gen = fCommon[VIRTIO_COMMON_CFGGEN];
		barrier();
		v = *reg32(fDevice, offset) |
		    (uint64_t)*reg32(fDevice, offset + 4) << 32;
		barrier();
	} while (gen != fCommon[VIRTIO_COMMON_CFGGEN]);
	return v;
}
