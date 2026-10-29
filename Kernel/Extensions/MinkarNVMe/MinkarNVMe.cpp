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

#include "MinkarNVMe.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <libkern/OSByteOrder.h>

#define super IOBlockStorageDevice
#define LOG(fmt, ...) IOLog("MinkarNVMe: " fmt "\n", ##__VA_ARGS__)

/* A slot's DMA area: the PRP list page, then the data. */
#define SLOT_DATA	kPage

OSDefineMetaClassAndStructors(MinkarNVMe, IOBlockStorageDevice);

static IOBufferMemoryDescriptor *
allocDMA(IOByteCount size)
{
	IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
	    kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
	    round_page(size), 0x00000000FFFFF000ULL);

	if (mem != NULL)
		bzero(mem->getBytesNoCopy(), round_page(size));
	return mem;
}

static inline void
barrier(void)
{
	__asm__ volatile("mfence" ::: "memory");
}

/* An identify string: ASCII, padded with spaces. */
static void
copyString(char *dst, size_t dstSize, const uint8_t *src, size_t len)
{
	size_t n = len < dstSize - 1 ? len : dstSize - 1;

	for (size_t i = 0; i < n; i++)
		dst[i] = (src[i] >= 0x20 && src[i] < 0x7F) ? (char)src[i] : ' ';
	while (n > 0 && dst[n - 1] == ' ')
		n--;
	dst[n] = '\0';
}

/* ---- Registers --------------------------------------------------------- */

/*
 * BAR0, uncached. When the platform made no IODeviceMemory for it, the
 * address and size come from the BAR itself (a 64-bit BAR continues in
 * BAR1).
 */
bool
MinkarNVMe::mapRegisters()
{
	const UInt8 reg = kIOPCIConfigBaseAddress0;

	fMap = fPCI->mapDeviceMemoryWithRegister(reg, kIOMapInhibitCache);
	if (fMap == NULL) {
		uint32_t lo = fPCI->configRead32(reg);
		uint64_t base = lo & ~0xFULL;
		uint32_t size;

		if ((lo & 1) != 0)
			return false;		/* I/O space */
		if ((lo & 0x6) == 0x4)
			base |= (uint64_t)fPCI->configRead32(reg + 4) << 32;
		if (base == 0)
			return false;
		uint16_t command = fPCI->configRead16(kIOPCIConfigCommand);

		fPCI->configWrite16(kIOPCIConfigCommand, command & ~kIOPCICommandMemorySpace);
		fPCI->configWrite32(reg, 0xFFFFFFFF);
		size = ~(fPCI->configRead32(reg) & ~0xFU) + 1;
		fPCI->configWrite32(reg, lo);
		fPCI->configWrite16(kIOPCIConfigCommand, command);
		if (size < 0x2000)
			size = 0x2000;	/* registers and the first doorbells */
		fDesc = IOMemoryDescriptor::withPhysicalAddress(base, size, kIODirectionNone);
		if (fDesc == NULL)
			return false;
		fMap = fDesc->map(kIOMapAnywhere | kIOMapInhibitCache);
		if (fMap == NULL)
			return false;
	}
	fRegs = (volatile uint8_t *)fMap->getVirtualAddress();
	return true;
}

uint32_t
MinkarNVMe::read32(uint32_t reg)
{
	return *(volatile uint32_t *)(fRegs + reg);
}

uint64_t
MinkarNVMe::read64(uint32_t reg)
{
	uint64_t lo = read32(reg);

	return lo | ((uint64_t)read32(reg + 4) << 32);
}

void
MinkarNVMe::write32(uint32_t reg, uint32_t value)
{
	*(volatile uint32_t *)(fRegs + reg) = value;
}

void
MinkarNVMe::write64(uint32_t reg, uint64_t value)
{
	write32(reg, (uint32_t)value);
	write32(reg + 4, (uint32_t)(value >> 32));
}

void
MinkarNVMe::ringSQ(Queue *q)
{
	barrier();
	write32(NVME_REG_DOORBELL + (2 * q->id) * fDoorbellStride, q->sqTail);
}

void
MinkarNVMe::ringCQ(Queue *q)
{
	write32(NVME_REG_DOORBELL + (2 * q->id + 1) * fDoorbellStride, q->cqHead);
}

/* ---- Controller -------------------------------------------------------- */

/* Waits for CSTS.RDY to read `ready`, up to the controller's timeout. */
bool
MinkarNVMe::waitReady(bool ready)
{
	for (uint32_t ms = 0; ms <= fTimeoutMS; ms++) {
		uint32_t csts = read32(NVME_REG_CSTS);

		if (csts == 0xFFFFFFFF)
			return false;	/* the device is gone */
		if (((csts & NVME_CSTS_RDY) != 0) == ready)
			return true;
		IOSleep(1);
	}
	return false;
}

bool
MinkarNVMe::resetController()
{
	uint32_t cc = read32(NVME_REG_CC);

	if (cc & NVME_CC_EN) {
		/* An enable in progress finishes before it is undone. */
		if ((read32(NVME_REG_CSTS) & NVME_CSTS_RDY) == 0)
			(void)waitReady(true);
		write32(NVME_REG_CC, cc & ~NVME_CC_EN);
	}
	return waitReady(false);
}

bool
MinkarNVMe::enableController()
{
	write32(NVME_REG_AQA, ((uint32_t)(fAdmin.entries - 1) << 16) | (fAdmin.entries - 1));
	write64(NVME_REG_ASQ, fAdmin.sqMem->getPhysicalAddress());
	write64(NVME_REG_ACQ, fAdmin.cqMem->getPhysicalAddress());
	write32(NVME_REG_CC, NVME_CC_EN | NVME_CC_CSS_NVM | NVME_CC_MPS(0) |
	    NVME_CC_AMS_RR | NVME_CC_IOSQES(6) | NVME_CC_IOCQES(4));
	if (!waitReady(true) || (read32(NVME_REG_CSTS) & NVME_CSTS_CFS)) {
		LOG("the controller does not become ready (CSTS 0x%x)", read32(NVME_REG_CSTS));
		return false;
	}
	fEnabled = true;
	return true;
}

/* Tells the controller to put its data in a safe state. */
void
MinkarNVMe::shutdownController()
{
	if (!fEnabled || fRegs == NULL)
		return;
	fEnabled = false;
	uint32_t cc = read32(NVME_REG_CC);

	write32(NVME_REG_CC, (cc & ~NVME_CC_SHN_MASK) | NVME_CC_SHN_NORMAL);
	for (uint32_t ms = 0; ms < 10000; ms++) {
		uint32_t csts = read32(NVME_REG_CSTS);

		if (csts == 0xFFFFFFFF ||
		    (csts & NVME_CSTS_SHST_MASK) == NVME_CSTS_SHST_DONE)
			return;
		IOSleep(1);
	}
	LOG("the controller did not finish shutting down");
}

bool
MinkarNVMe::allocQueue(Queue *q, uint16_t id, uint16_t entries)
{
	bzero(q, sizeof(*q));
	q->sqMem = allocDMA((IOByteCount)entries * sizeof(struct nvme_sqe));
	q->cqMem = allocDMA((IOByteCount)entries * sizeof(struct nvme_cqe));
	if (q->sqMem == NULL || q->cqMem == NULL)
		return false;
	q->sq = (volatile struct nvme_sqe *)q->sqMem->getBytesNoCopy();
	q->cq = (volatile struct nvme_cqe *)q->cqMem->getBytesNoCopy();
	q->entries = entries;
	q->phase = 1;
	q->id = id;
	return true;
}

void
MinkarNVMe::freeQueue(Queue *q)
{
	OSSafeReleaseNULL(q->sqMem);
	OSSafeReleaseNULL(q->cqMem);
	q->sq = NULL;
	q->cq = NULL;
}

/* An admin command, waited for by polling (while the driver starts). */
IOReturn
MinkarNVMe::adminCommand(struct nvme_sqe *cmd, uint32_t *result)
{
	Queue *q = &fAdmin;

	cmd->cid = fAdminCID++;
	bcopy(cmd, (void *)(uintptr_t)&q->sq[q->sqTail], sizeof(*cmd));
	q->sqTail = (uint16_t)((q->sqTail + 1) % q->entries);
	ringSQ(q);

	for (uint32_t us = 0; us < (fTimeoutMS + 5000) * 1000; us += 10) {
		volatile struct nvme_cqe *cqe = &q->cq[q->cqHead];
		uint16_t status = cqe->status;

		if (NVME_CQE_PHASE(status) != q->phase) {
			IODelay(10);
			continue;
		}
		uint16_t cid = cqe->cid;
		uint32_t dw0 = cqe->dw0;

		if (++q->cqHead == q->entries) {
			q->cqHead = 0;
			q->phase ^= 1;
		}
		ringCQ(q);
		if (cid != cmd->cid)
			continue;	/* a stale completion */
		if (NVME_CQE_SC(status) != 0 || NVME_CQE_SCT(status) != 0) {
			LOG("admin command 0x%02x failed: type %u, status 0x%02x",
			    cmd->opc, NVME_CQE_SCT(status), NVME_CQE_SC(status));
			return kIOReturnIOError;
		}
		if (result != NULL)
			*result = dw0;
		return kIOReturnSuccess;
	}
	LOG("admin command 0x%02x timed out", cmd->opc);
	return kIOReturnTimeout;
}

bool
MinkarNVMe::identify()
{
	struct nvme_sqe cmd;
	const uint8_t *id = (const uint8_t *)fIdentify->getBytesNoCopy();

	bzero(&cmd, sizeof(cmd));
	cmd.opc = NVME_ADM_IDENTIFY;
	cmd.prp1 = fIdentify->getPhysicalAddress();
	cmd.cdw10 = NVME_CNS_CONTROLLER;
	if (adminCommand(&cmd, NULL) != kIOReturnSuccess)
		return false;
	copyString(fModel, sizeof(fModel), id + NVME_IDC_MN, 40);
	copyString(fSerial, sizeof(fSerial), id + NVME_IDC_SN, 20);
	copyString(fFirmware, sizeof(fFirmware), id + NVME_IDC_FR, 8);
	uint8_t mdts = id[NVME_IDC_MDTS];
	uint32_t nn = OSReadLittleInt32(id, NVME_IDC_NN);
	fWriteCache = (id[NVME_IDC_VWC] & 1) != 0;

	fMaxBytes = kMaxBytes;
	if (mdts != 0 && mdts < 20 && ((UInt32)kPage << mdts) < fMaxBytes)
		fMaxBytes = (UInt32)kPage << mdts;
	if (nn < 1) {
		LOG("%s: no namespaces", fModel);
		return false;
	}

	bzero(&cmd, sizeof(cmd));
	cmd.opc = NVME_ADM_IDENTIFY;
	cmd.nsid = 1;
	cmd.prp1 = fIdentify->getPhysicalAddress();
	cmd.cdw10 = NVME_CNS_NAMESPACE;
	bzero(fIdentify->getBytesNoCopy(), kPage);
	if (adminCommand(&cmd, NULL) != kIOReturnSuccess)
		return false;
	fBlocks = OSReadLittleInt64(id, NVME_IDN_NSZE);
	unsigned format = id[NVME_IDN_FLBAS] & 0xF;
	uint32_t lbaf = OSReadLittleInt32(id, NVME_IDN_LBAF + 4 * format);
	unsigned lbads = NVME_LBAF_LBADS(lbaf);

	if (fBlocks == 0) {
		LOG("%s: namespace 1 is not active", fModel);
		return false;
	}
	if (lbads < 9 || lbads > 12) {
		LOG("%s: unsupported block size 2^%u", fModel, lbads);
		return false;
	}
	fBlockSize = 1U << lbads;
	return true;
}

bool
MinkarNVMe::createIOQueues()
{
	struct nvme_sqe cmd;
	uint64_t cap = read64(NVME_REG_CAP);
	uint32_t entries = NVME_CAP_MQES(cap) + 1;

	if (entries > kIOEntries)
		entries = kIOEntries;
	if (entries < 2 || !allocQueue(&fIO, 1, (uint16_t)entries))
		return false;
	fNumSlots = entries - 1 < kSlots ? entries - 1 : kSlots;

	/* One submission and one completion queue (both counts 0-based). */
	bzero(&cmd, sizeof(cmd));
	cmd.opc = NVME_ADM_SET_FEATURES;
	cmd.cdw10 = NVME_FEAT_NUM_QUEUES;
	cmd.cdw11 = 0;
	(void)adminCommand(&cmd, NULL);

	bzero(&cmd, sizeof(cmd));
	cmd.opc = NVME_ADM_CREATE_CQ;
	cmd.prp1 = fIO.cqMem->getPhysicalAddress();
	cmd.cdw10 = ((uint32_t)(fIO.entries - 1) << 16) | fIO.id;
	cmd.cdw11 = NVME_QUEUE_PC | NVME_CQ_IEN;	/* vector 0 */
	if (adminCommand(&cmd, NULL) != kIOReturnSuccess)
		return false;

	bzero(&cmd, sizeof(cmd));
	cmd.opc = NVME_ADM_CREATE_SQ;
	cmd.prp1 = fIO.sqMem->getPhysicalAddress();
	cmd.cdw10 = ((uint32_t)(fIO.entries - 1) << 16) | fIO.id;
	cmd.cdw11 = ((uint32_t)fIO.id << 16) | NVME_QUEUE_PC;
	return adminCommand(&cmd, NULL) == kIOReturnSuccess;
}

/* ---- Start and stop ---------------------------------------------------- */

bool
MinkarNVMe::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	if (!setupDevice()) {
		freeResources();
		return false;
	}
	fStarted = true;
	registerService();
	return true;
}

bool
MinkarNVMe::setupDevice()
{
	fPCI->setMemoryEnable(true);
	fPCI->setBusMasterEnable(true);
	if (!mapRegisters()) {
		LOG("cannot map the controller's registers (BAR0 0x%08x)",
		    fPCI->configRead32(kIOPCIConfigBaseAddress0));
		return false;
	}
	uint64_t cap = read64(NVME_REG_CAP);
	uint32_t vs = read32(NVME_REG_VS);

	if (cap == ~0ULL) {
		LOG("the controller does not answer");
		return false;
	}
	fDoorbellStride = 4U << NVME_CAP_DSTRD(cap);
	fTimeoutMS = NVME_CAP_TO(cap) * 500;
	if (fTimeoutMS < 500)
		fTimeoutMS = 500;
	if (!NVME_CAP_CSS_NVM(cap) || NVME_CAP_MPSMIN(cap) != 0) {
		LOG("unsupported controller (CAP 0x%llx)", cap);
		return false;
	}

	if (!resetController()) {
		LOG("the controller does not reset (CSTS 0x%x)", read32(NVME_REG_CSTS));
		return false;
	}
	fIdentify = allocDMA(kPage);
	if (fIdentify == NULL || !allocQueue(&fAdmin, 0, kAdminEntries) ||
	    !enableController() || !identify())
		return false;
	if (!createIOQueues()) {
		LOG("%s: cannot create the I/O queues", fModel);
		return false;
	}
	for (unsigned i = 0; i < fNumSlots; i++) {
		fSlots[i].mem = allocDMA(SLOT_DATA + fMaxBytes);
		if (fSlots[i].mem == NULL)
			return false;
		fSlots[i].bytes = (uint8_t *)fSlots[i].mem->getBytesNoCopy();
		fSlots[i].phys = fSlots[i].mem->getPhysicalAddress();
	}

	fWorkLoop = IOWorkLoop::workLoop();
	fGate = IOCommandGate::commandGate(this);
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this,
	    &MinkarNVMe::timerFired));
	if (fWorkLoop == NULL || fGate == NULL || fTimer == NULL ||
	    fWorkLoop->addEventSource(fGate) != kIOReturnSuccess ||
	    fWorkLoop->addEventSource(fTimer) != kIOReturnSuccess)
		return false;

	/*
	 * MSI (or MSI-X) when the device offers it, else its pin interrupt;
	 * each is kept only if a read shows it arrives. Without either, the
	 * timer collects the completions.
	 */
	fStarted = true;	/* for the test reads */
	const char *how = "polling";
	int messaged = -1;
	for (int i = 0; i < 8; i++) {
		int type;

		if (fPCI->getInterruptType(i, &type) != kIOReturnSuccess)
			break;
		if (type & kIOInterruptTypePCIMessaged) {
			messaged = i;
			break;
		}
	}
	if (messaged > 0 && tryInterrupt(messaged))
		how = "MSI";
	else if (tryInterrupt(0))
		how = "pin interrupt";
	fStarted = false;

	/* The largest request the storage stack may send us. */
	setProperty(kIOMaximumBlockCountReadKey, (UInt64)(fMaxBytes / fBlockSize), 64);
	setProperty(kIOMaximumBlockCountWriteKey, (UInt64)(fMaxBytes / fBlockSize), 64);
	setProperty(kIOMaximumByteCountReadKey, (UInt64)fMaxBytes, 64);
	setProperty(kIOMaximumByteCountWriteKey, (UInt64)fMaxBytes, 64);
	setProperty("Serial Number", fSerial);

	LOG("%s (firmware %s), NVMe %u.%u, %llu blocks of %u bytes, %s, queue of %u%s",
	    fModel, fFirmware, vs >> 16, (vs >> 8) & 0xFF, fBlocks, fBlockSize, how,
	    fIO.entries, fWriteCache ? ", write cache" : "");
	return true;
}

void
MinkarNVMe::freeResources()
{
	if (fWorkLoop != NULL) {
		if (fIntSource != NULL) {
			fIntSource->disable();
			fWorkLoop->removeEventSource(fIntSource);
		}
		if (fTimer != NULL) {
			fTimer->cancelTimeout();
			fWorkLoop->removeEventSource(fTimer);
		}
		if (fGate != NULL)
			fWorkLoop->removeEventSource(fGate);
	}
	shutdownController();
	OSSafeReleaseNULL(fIntSource);
	OSSafeReleaseNULL(fTimer);
	OSSafeReleaseNULL(fGate);
	OSSafeReleaseNULL(fWorkLoop);
	for (unsigned i = 0; i < kSlots; i++)
		OSSafeReleaseNULL(fSlots[i].mem);
	freeQueue(&fIO);
	freeQueue(&fAdmin);
	OSSafeReleaseNULL(fIdentify);
	fRegs = NULL;
	OSSafeReleaseNULL(fMap);
	OSSafeReleaseNULL(fDesc);
}

void
MinkarNVMe::stop(IOService *provider)
{
	fStarted = false;
	freeResources();
	super::stop(provider);
}

void
MinkarNVMe::free()
{
	freeResources();
	OSSafeReleaseNULL(fPCI);
	super::free();
}

IOWorkLoop *
MinkarNVMe::getWorkLoop() const
{
	return fWorkLoop;
}

/* Power off and restart: the controller saves its cache first. */
void
MinkarNVMe::systemWillShutdown(IOOptionBits specifier)
{
	if (specifier == kIOMessageSystemWillPowerOff ||
	    specifier == kIOMessageSystemWillRestart) {
		fStarted = false;
		shutdownController();
	}
	super::systemWillShutdown(specifier);
}

/* ---- Requests (on the work loop) --------------------------------------- */

bool
MinkarNVMe::submit(Request *req, unsigned index)
{
	Slot *slot = &fSlots[index];
	struct nvme_sqe cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.opc = req->opcode;
	cmd.cid = (uint16_t)index;
	cmd.nsid = 1;
	if (req->opcode != NVME_CMD_FLUSH) {
		uint64_t data = slot->phys + SLOT_DATA;
		unsigned pages = (req->bytes + kPage - 1) / kPage;

		if (req->opcode == NVME_CMD_WRITE &&
		    req->buffer->readBytes(0, slot->bytes + SLOT_DATA, req->bytes) != req->bytes)
			return false;
		cmd.prp1 = data;
		if (pages == 2) {
			cmd.prp2 = data + kPage;
		} else if (pages > 2) {
			uint64_t *list = (uint64_t *)slot->bytes;

			for (unsigned i = 1; i < pages; i++)
				list[i - 1] = data + (uint64_t)i * kPage;
			cmd.prp2 = slot->phys;
		}
		cmd.cdw10 = (uint32_t)req->lba;
		cmd.cdw11 = (uint32_t)(req->lba >> 32);
		cmd.cdw12 = req->bytes / fBlockSize - 1;
	}
	bcopy(&cmd, (void *)(uintptr_t)&fIO.sq[fIO.sqTail], sizeof(cmd));
	fIO.sqTail = (uint16_t)((fIO.sqTail + 1) % fIO.entries);
	slot->request = req;
	fInFlight++;
	ringSQ(&fIO);
	if (fIntSource == NULL || fInFlight == 1)
		fTimer->setTimeoutMS(kPollMS);
	return true;
}

void
MinkarNVMe::queueRequest(Request *req)
{
	req->next = NULL;
	if (fPendingTail != NULL)
		fPendingTail->next = req;
	else
		fPendingHead = req;
	fPendingTail = req;
	submitPending();
}

void
MinkarNVMe::submitPending()
{
	while (fPendingHead != NULL) {
		unsigned index = fNumSlots;

		for (unsigned i = 0; i < fNumSlots; i++) {
			if (fSlots[i].request == NULL) {
				index = i;
				break;
			}
		}
		if (index == fNumSlots)
			return;
		Request *req = fPendingHead;
		fPendingHead = req->next;
		if (fPendingHead == NULL)
			fPendingTail = NULL;
		if (!fStarted && !req->sync)
			complete(req, kIOReturnNotAttached, 0);
		else if (!submit(req, index))
			complete(req, kIOReturnIOError, 0);
	}
}

void
MinkarNVMe::complete(Request *req, IOReturn status, UInt64 bytes)
{
	if (req->sync) {
		req->status = status;
		req->done = true;
		fGate->commandWakeup(req);
		return;
	}
	IOMemoryDescriptor *buffer = req->buffer;
	IOStorageCompletion completion = req->completion;

	IOFree(req, sizeof(*req));
	IOStorage::complete(&completion, status, bytes);
	if (buffer != NULL)
		buffer->release();
}

void
MinkarNVMe::serviceQueue()
{
	bool any = false;

	if (fIO.cq == NULL)
		return;
	for (;;) {
		volatile struct nvme_cqe *cqe = &fIO.cq[fIO.cqHead];
		uint16_t status = cqe->status;

		if (NVME_CQE_PHASE(status) != fIO.phase)
			break;
		uint16_t cid = cqe->cid;

		if (++fIO.cqHead == fIO.entries) {
			fIO.cqHead = 0;
			fIO.phase ^= 1;
		}
		any = true;
		if (cid >= fNumSlots || fSlots[cid].request == NULL)
			continue;
		Slot *slot = &fSlots[cid];
		Request *req = slot->request;
		IOReturn result = kIOReturnSuccess;

		slot->request = NULL;
		if (fInFlight > 0)
			fInFlight--;
		fCompletions++;
		if (fInTimer)
			fPolled++;
		if (NVME_CQE_SC(status) != 0 || NVME_CQE_SCT(status) != 0) {
			LOG("command 0x%02x at block %llu failed: type %u, status 0x%02x",
			    req->opcode, req->lba, NVME_CQE_SCT(status), NVME_CQE_SC(status));
			result = kIOReturnIOError;
		} else if (req->opcode == NVME_CMD_READ &&
		    req->buffer->writeBytes(0, slot->bytes + SLOT_DATA, req->bytes) != req->bytes) {
			result = kIOReturnIOError;
		}
		complete(req, result, result == kIOReturnSuccess ? req->bytes : 0);
	}
	if (any)
		ringCQ(&fIO);
	submitPending();
}

/*
 * Uses interrupt `index` if a read of the first block brings one; else
 * leaves no interrupt source.
 */
bool
MinkarNVMe::tryInterrupt(int index)
{
	fIntSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventSource::Action, this,
	    &MinkarNVMe::interruptOccurred), fPCI, index);
	if (fIntSource == NULL)
		return false;
	if (fWorkLoop->addEventSource(fIntSource) != kIOReturnSuccess) {
		OSSafeReleaseNULL(fIntSource);
		return false;
	}
	fIntSource->enable();

	IOBufferMemoryDescriptor *buf = IOBufferMemoryDescriptor::withCapacity(
	    fBlockSize, kIODirectionIn);
	UInt64 before = fInterrupts;
	bool works = false;

	if (buf != NULL) {
		Request req;

		bzero(&req, sizeof(req));
		buf->setLength(fBlockSize);
		req.buffer = buf;
		req.bytes = fBlockSize;
		req.opcode = NVME_CMD_READ;
		if (runSync(&req) == kIOReturnSuccess) {
			IOSleep(20);	/* an interrupt later than the timer */
			works = fInterrupts > before;
		}
		buf->release();
	}
	if (!works) {
		fIntSource->disable();
		fWorkLoop->removeEventSource(fIntSource);
		OSSafeReleaseNULL(fIntSource);
	}
	return works;
}

void
MinkarNVMe::interruptOccurred(IOInterruptEventSource *, int)
{
	fInterrupts++;
	serviceQueue();
}

/* How completions arrive, for ioreg: by interrupt or by the timer. */
void
MinkarNVMe::publishCounts()
{
	setProperty("Interrupts", fInterrupts, 64);
	setProperty("Completions", fCompletions, 64);
	setProperty("Completions by timer", fPolled, 64);
}

/* A safety net for a lost interrupt, and the work itself without one. */
void
MinkarNVMe::timerFired(IOTimerEventSource *)
{
	fInTimer = true;
	serviceQueue();
	fInTimer = false;
	publishCounts();
	if (fInFlight > 0)
		fTimer->setTimeoutMS(kPollMS);
}

IOReturn
MinkarNVMe::gatedEnqueue(OSObject *owner, void *req, void *, void *, void *)
{
	((MinkarNVMe *)owner)->queueRequest((Request *)req);
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::gatedSync(OSObject *owner, void *arg, void *, void *, void *)
{
	MinkarNVMe *self = (MinkarNVMe *)owner;
	Request *req = (Request *)arg;

	self->queueRequest(req);
	while (!req->done)
		self->fGate->commandSleep(req, THREAD_UNINT);
	return req->status;
}

IOReturn
MinkarNVMe::runSync(Request *req)
{
	req->sync = true;
	req->done = false;
	return fGate->runAction(gatedSync, req);
}

/* ---- IOBlockStorageDevice ---------------------------------------------- */

IOReturn
MinkarNVMe::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *, IOStorageCompletion *completion)
{
	if (!fStarted)
		return kIOReturnNotAttached;
	if (buffer == NULL || completion == NULL || nblks == 0)
		return kIOReturnBadArgument;
	UInt64 bytes = nblks * fBlockSize;
	if (bytes > fMaxBytes || block + nblks > fBlocks || buffer->getLength() < bytes)
		return kIOReturnBadArgument;

	Request *req = (Request *)IOMalloc(sizeof(Request));
	if (req == NULL)
		return kIOReturnNoMemory;
	bzero(req, sizeof(*req));
	req->buffer = buffer;
	buffer->retain();
	req->lba = block;
	req->bytes = (UInt32)bytes;
	req->opcode = buffer->getDirection() == kIODirectionOut ? NVME_CMD_WRITE : NVME_CMD_READ;
	req->completion = *completion;
	fGate->runAction(gatedEnqueue, req);
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::doSynchronize(UInt64, UInt64, IOStorageSynchronizeOptions)
{
	if (!fStarted)
		return kIOReturnNotAttached;
	if (!fWriteCache)
		return kIOReturnSuccess;	/* no volatile cache to flush */
	Request req;

	bzero(&req, sizeof(req));
	req.opcode = NVME_CMD_FLUSH;
	return runSync(&req);
}

IOReturn
MinkarNVMe::doEjectMedia(void)
{
	return kIOReturnUnsupported;
}

IOReturn
MinkarNVMe::doFormatMedia(UInt64)
{
	return kIOReturnUnsupported;
}

UInt32
MinkarNVMe::doGetFormatCapacities(UInt64 *capacities, UInt32 max) const
{
	if (capacities != NULL && max > 0)
		capacities[0] = fBlocks * fBlockSize;
	return 1;
}

char *
MinkarNVMe::getVendorString(void)
{
	static char vendor[] = "NVMe";

	return vendor;
}

char *
MinkarNVMe::getProductString(void)
{
	return fModel;
}

char *
MinkarNVMe::getRevisionString(void)
{
	return fFirmware;
}

char *
MinkarNVMe::getAdditionalDeviceInfoString(void)
{
	return NULL;
}

IOReturn
MinkarNVMe::reportBlockSize(UInt64 *blockSize)
{
	*blockSize = fBlockSize;
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::reportEjectability(bool *isEjectable)
{
	*isEjectable = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::reportMaxValidBlock(UInt64 *maxBlock)
{
	*maxBlock = fBlocks > 0 ? fBlocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = fBlocks > 0;
	if (changedState != NULL)
		*changedState = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::reportRemovability(bool *isRemovable)
{
	*isRemovable = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarNVMe::reportWriteProtection(bool *isWriteProtected)
{
	*isWriteProtected = false;
	return kIOReturnSuccess;
}
