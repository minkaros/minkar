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
 * ACPIOSLayer: the operating system services ACPICA needs (acpiosxf.h),
 * on the XNU kernel and IOKit: memory and physical mappings, locks,
 * semaphores, threads, port and PCI configuration I/O, the timer, the FACS
 * global lock and the SCI interrupt.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOInterrupts.h>
#include <libkern/OSAtomic.h>
#include <kern/thread.h>
#include <mach/mach_time.h>

extern "C" {
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
}

#include "ACPIOSLayer.h"

/* ---- Initialization ---------------------------------------------------- */

static ACPI_PHYSICAL_ADDRESS gRSDP;
static IOLock *gMapLock;

void
ACPIOSLayerSetRootPointer(uint64_t rsdp)
{
	gRSDP = rsdp;
}

ACPI_STATUS
AcpiOsInitialize(void)
{
	if (gMapLock == NULL)
		gMapLock = IOLockAlloc();
	return gMapLock != NULL ? AE_OK : AE_NO_MEMORY;
}

ACPI_STATUS
AcpiOsTerminate(void)
{
	return AE_OK;
}

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(void)
{
	return gRSDP;
}

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal)
{
	(void)InitVal;
	*NewVal = NULL;
	return AE_OK;
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	(void)ExistingTable;
	*NewTable = NULL;
	return AE_OK;
}

ACPI_STATUS
AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
    ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
	(void)ExistingTable;
	*NewAddress = 0;
	*NewTableLength = 0;
	return AE_OK;
}

/* ---- Memory ------------------------------------------------------------ */

/* Each block starts with its size, so AcpiOsFree can give it back. */
void *
AcpiOsAllocate(ACPI_SIZE Size)
{
	size_t *p = (size_t *)IOMalloc(Size + 16);

	if (p == NULL)
		return NULL;
	p[0] = Size + 16;
	return (uint8_t *)p + 16;
}

void
AcpiOsFree(void *Memory)
{
	if (Memory != NULL) {
		size_t *p = (size_t *)(void *)((uint8_t *)Memory - 16);

		IOFree(p, p[0]);
	}
}

/* Physical mappings, by virtual address, so AcpiOsUnmapMemory finds them. */
struct Mapping {
	struct Mapping *	next;
	void *			virt;
	IOMemoryMap *		map;
};
static struct Mapping *gMappings;

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
	IOMemoryDescriptor *desc;
	IOMemoryMap *map;
	struct Mapping *m;

	desc = IOMemoryDescriptor::withPhysicalAddress((IOPhysicalAddress)Where,
	    (IOByteCount)Length, kIODirectionInOut);
	if (desc == NULL)
		return NULL;
	map = desc->map(kIOMapAnywhere);
	desc->release();
	if (map == NULL)
		return NULL;
	m = IONew(struct Mapping, 1);
	if (m == NULL) {
		map->release();
		return NULL;
	}
	m->virt = (void *)map->getVirtualAddress();
	m->map = map;
	IOLockLock(gMapLock);
	m->next = gMappings;
	gMappings = m;
	IOLockUnlock(gMapLock);
	return m->virt;
}

void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
	struct Mapping **pp, *m = NULL;

	(void)Size;
	IOLockLock(gMapLock);
	for (pp = &gMappings; *pp != NULL; pp = &(*pp)->next) {
		if ((*pp)->virt == LogicalAddress) {
			m = *pp;
			*pp = m->next;
			break;
		}
	}
	IOLockUnlock(gMapLock);
	if (m != NULL) {
		m->map->release();
		IODelete(m, struct Mapping, 1);
	}
}

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
	void *p = AcpiOsMapMemory(Address, Width / 8);

	if (p == NULL)
		return AE_NO_MEMORY;
	switch (Width) {
	case 8:  *Value = *(volatile UINT8 *)p; break;
	case 16: *Value = *(volatile UINT16 *)p; break;
	case 32: *Value = *(volatile UINT32 *)p; break;
	case 64: *Value = *(volatile UINT64 *)p; break;
	default: AcpiOsUnmapMemory(p, Width / 8); return AE_BAD_PARAMETER;
	}
	AcpiOsUnmapMemory(p, Width / 8);
	return AE_OK;
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
	void *p = AcpiOsMapMemory(Address, Width / 8);

	if (p == NULL)
		return AE_NO_MEMORY;
	switch (Width) {
	case 8:  *(volatile UINT8 *)p = (UINT8)Value; break;
	case 16: *(volatile UINT16 *)p = (UINT16)Value; break;
	case 32: *(volatile UINT32 *)p = (UINT32)Value; break;
	case 64: *(volatile UINT64 *)p = Value; break;
	default: AcpiOsUnmapMemory(p, Width / 8); return AE_BAD_PARAMETER;
	}
	AcpiOsUnmapMemory(p, Width / 8);
	return AE_OK;
}

/* ---- Spin locks, semaphores --------------------------------------------- */

ACPI_STATUS
AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
	IOSimpleLock *lock = IOSimpleLockAlloc();

	if (lock == NULL)
		return AE_NO_MEMORY;
	*OutHandle = lock;
	return AE_OK;
}

void
AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
	if (Handle != NULL)
		IOSimpleLockFree((IOSimpleLock *)Handle);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
	return (ACPI_CPU_FLAGS)IOSimpleLockLockDisableInterrupt((IOSimpleLock *)Handle);
}

void
AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
	IOSimpleLockUnlockEnableInterrupt((IOSimpleLock *)Handle, (IOInterruptState)Flags);
}

struct Semaphore {
	IOLock *	lock;
	UINT32		units;
	UINT32		max;
};

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
	struct Semaphore *s;

	if (OutHandle == NULL || InitialUnits > MaxUnits)
		return AE_BAD_PARAMETER;
	if ((s = IONew(struct Semaphore, 1)) == NULL)
		return AE_NO_MEMORY;
	if ((s->lock = IOLockAlloc()) == NULL) {
		IODelete(s, struct Semaphore, 1);
		return AE_NO_MEMORY;
	}
	s->units = InitialUnits;
	s->max = MaxUnits;
	*OutHandle = s;
	return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
	struct Semaphore *s = (struct Semaphore *)Handle;

	if (s == NULL)
		return AE_BAD_PARAMETER;
	IOLockFree(s->lock);
	IODelete(s, struct Semaphore, 1);
	return AE_OK;
}

/* Timeout in milliseconds; ACPI_WAIT_FOREVER waits forever, 0 not at all. */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
	struct Semaphore *s = (struct Semaphore *)Handle;
	uint64_t deadline = 0;
	ACPI_STATUS ret = AE_OK;

	if (s == NULL || Units == 0)
		return AE_BAD_PARAMETER;
	if (Timeout != ACPI_WAIT_FOREVER && Timeout != 0)
		clock_interval_to_deadline(Timeout, kMillisecondScale, &deadline);
	IOLockLock(s->lock);
	while (s->units < Units) {
		if (Timeout == 0) {
			ret = AE_TIME;
			break;
		}
		if (Timeout == ACPI_WAIT_FOREVER) {
			IOLockSleep(s->lock, s, THREAD_UNINT);
		} else if (IOLockSleepDeadline(s->lock, s, deadline, THREAD_UNINT) ==
		    THREAD_TIMED_OUT && s->units < Units) {
			ret = AE_TIME;
			break;
		}
	}
	if (ret == AE_OK)
		s->units -= Units;
	IOLockUnlock(s->lock);
	return ret;
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
	struct Semaphore *s = (struct Semaphore *)Handle;

	if (s == NULL)
		return AE_BAD_PARAMETER;
	IOLockLock(s->lock);
	if (s->units + Units > s->max) {
		IOLockUnlock(s->lock);
		return AE_LIMIT;
	}
	s->units += Units;
	IOLockWakeup(s->lock, s, false);
	IOLockUnlock(s->lock);
	return AE_OK;
}

/* ---- Threads, time ------------------------------------------------------ */

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
	return (ACPI_THREAD_ID)(uintptr_t)current_thread();
}

struct ExecuteArgs {
	ACPI_OSD_EXEC_CALLBACK	function;
	void *			context;
};
static volatile SInt32 gPendingExecutes;

static void
executeThread(void *arg, wait_result_t)
{
	struct ExecuteArgs *a = (struct ExecuteArgs *)arg;

	a->function(a->context);
	IODelete(a, struct ExecuteArgs, 1);
	OSDecrementAtomic(&gPendingExecutes);
	/* The thread ends when this function returns. */
}

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
	struct ExecuteArgs *a;
	thread_t thread;

	(void)Type;
	if (Function == NULL)
		return AE_BAD_PARAMETER;
	if ((a = IONew(struct ExecuteArgs, 1)) == NULL)
		return AE_NO_MEMORY;
	a->function = Function;
	a->context = Context;
	OSIncrementAtomic(&gPendingExecutes);
	if (kernel_thread_start(executeThread, a, &thread) != KERN_SUCCESS) {
		OSDecrementAtomic(&gPendingExecutes);
		IODelete(a, struct ExecuteArgs, 1);
		return AE_ERROR;
	}
	thread_deallocate(thread);
	return AE_OK;
}

void
AcpiOsWaitEventsComplete(void)
{
	while (gPendingExecutes > 0)
		IOSleep(1);
}

void
AcpiOsSleep(UINT64 Milliseconds)
{
	IOSleep((unsigned)Milliseconds);
}

void
AcpiOsStall(UINT32 Microseconds)
{
	IODelay(Microseconds);
}

/* In 100-nanosecond units. */
UINT64
AcpiOsGetTimer(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns / 100;
}

/* ---- Port and PCI configuration I/O ------------------------------------- */

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
	uint16_t port = (uint16_t)Address;

	switch (Width) {
	case 8: {
		uint8_t v;
		__asm__ __volatile__("inb %w1, %b0" : "=a"(v) : "Nd"(port));
		*Value = v;
		break;
	}
	case 16: {
		uint16_t v;
		__asm__ __volatile__("inw %w1, %w0" : "=a"(v) : "Nd"(port));
		*Value = v;
		break;
	}
	case 32: {
		uint32_t v;
		__asm__ __volatile__("inl %w1, %0" : "=a"(v) : "Nd"(port));
		*Value = v;
		break;
	}
	default:
		return AE_BAD_PARAMETER;
	}
	return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
	uint16_t port = (uint16_t)Address;

	switch (Width) {
	case 8:  __asm__ __volatile__("outb %b0, %w1" : : "a"((uint8_t)Value), "Nd"(port)); break;
	case 16: __asm__ __volatile__("outw %w0, %w1" : : "a"((uint16_t)Value), "Nd"(port)); break;
	case 32: __asm__ __volatile__("outl %0, %w1" : : "a"(Value), "Nd"(port)); break;
	default: return AE_BAD_PARAMETER;
	}
	return AE_OK;
}

/* PCI configuration mechanism #1 (ports 0xCF8/0xCFC), registers 0-255. */
static IOSimpleLock *gPCILock;

static uint32_t
pciAddress(ACPI_PCI_ID *id, UINT32 reg)
{
	return 0x80000000U | ((uint32_t)id->Bus << 16) | ((uint32_t)id->Device << 11) |
	    ((uint32_t)id->Function << 8) | (reg & 0xFC);
}

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width)
{
	IOInterruptState is;
	UINT32 v;

	if (PciId->Segment != 0 || Reg > 255 || (Width != 8 && Width != 16 && Width != 32))
		return AE_BAD_PARAMETER;
	if (gPCILock == NULL)
		gPCILock = IOSimpleLockAlloc();
	is = IOSimpleLockLockDisableInterrupt(gPCILock);
	AcpiOsWritePort(0xCF8, pciAddress(PciId, Reg), 32);
	AcpiOsReadPort(0xCFC + (Reg & 3), &v, Width);
	IOSimpleLockUnlockEnableInterrupt(gPCILock, is);
	*Value = v;
	return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
	IOInterruptState is;

	if (PciId->Segment != 0 || Reg > 255 || (Width != 8 && Width != 16 && Width != 32))
		return AE_BAD_PARAMETER;
	if (gPCILock == NULL)
		gPCILock = IOSimpleLockAlloc();
	is = IOSimpleLockLockDisableInterrupt(gPCILock);
	AcpiOsWritePort(0xCF8, pciAddress(PciId, Reg), 32);
	AcpiOsWritePort(0xCFC + (Reg & 3), (UINT32)Value, Width);
	IOSimpleLockUnlockEnableInterrupt(gPCILock, is);
	return AE_OK;
}

/* ---- The FACS global lock (ACPI 6.5, 5.2.10.1) -------------------------- */

#define GL_PENDING	0x1
#define GL_OWNED	0x2

/* Returns nonzero when the lock is acquired, zero when it is pending. */
int
AcpiOsAcquireGlobalLock(void *FacsGlobalLock)
{
	volatile UInt32 *lock = (volatile UInt32 *)FacsGlobalLock;
	UInt32 old, val;

	do {
		old = *lock;
		val = (old & ~GL_PENDING) | GL_OWNED;
		if (old & GL_OWNED)
			val |= GL_PENDING;
	} while (!OSCompareAndSwap(old, val, lock));
	return (val & GL_PENDING) == 0;
}

/* Returns nonzero when the firmware waits for the lock (signal GBL_RLS). */
int
AcpiOsReleaseGlobalLock(void *FacsGlobalLock)
{
	volatile UInt32 *lock = (volatile UInt32 *)FacsGlobalLock;
	UInt32 old;

	do {
		old = *lock;
	} while (!OSCompareAndSwap(old, old & ~(GL_PENDING | GL_OWNED), lock));
	return (old & GL_PENDING) != 0;
}

/* ---- The SCI ------------------------------------------------------------ */

/*
 * The SCI is a GSI on the IOAPIC: a nub carries its specifier (vector and
 * trigger flags, as AppleAPIC reads them) and the handler runs in primary
 * interrupt context, as ACPICA expects.
 */
struct SCIHandler {
	ACPI_OSD_HANDLER	handler;
	void *			context;
	IOService *		nub;
};
static struct SCIHandler gSCI;

static void
sciInterrupt(OSObject *, void *, IOService *, int)
{
	if (gSCI.handler != NULL)
		gSCI.handler(gSCI.context);
}

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine,
    void *Context)
{
	IOService *nub;
	uint32_t gsi, flags;

	if (ServiceRoutine == NULL)
		return AE_BAD_PARAMETER;
	if (gSCI.handler != NULL)
		return AE_ALREADY_EXISTS;
	ACPIOSLayerSCIRoute(InterruptNumber, &gsi, &flags);
	nub = ACPIOSLayerCreateInterruptNub("acpi-sci", gsi, flags);
	if (nub == NULL)
		return AE_ERROR;
	gSCI.handler = ServiceRoutine;
	gSCI.context = Context;
	gSCI.nub = nub;
	if (nub->registerInterrupt(0, NULL, (IOInterruptAction)sciInterrupt, NULL) !=
	    kIOReturnSuccess || nub->enableInterrupt(0) != kIOReturnSuccess) {
		IOLog("ACPI: cannot register the SCI (GSI %u)\n", gsi);
		gSCI.handler = NULL;
		return AE_ERROR;
	}
	IOLog("ACPI: SCI on GSI %u (%s, active %s)\n", gsi,
	    (flags & 1) ? "level" : "edge", (flags & 2) ? "low" : "high");
	return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine)
{
	(void)InterruptNumber;
	if (ServiceRoutine != gSCI.handler || gSCI.nub == NULL)
		return AE_NOT_EXIST;
	gSCI.nub->disableInterrupt(0);
	gSCI.nub->unregisterInterrupt(0);
	gSCI.handler = NULL;
	return AE_OK;
}

/* ---- Output, signals, sleep --------------------------------------------- */

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf(const char *Format, ...)
{
	va_list ap;

	va_start(ap, Format);
	AcpiOsVprintf(Format, ap);
	va_end(ap);
}

/* ACPICA prints in pieces: whole lines go to the kernel log. */
void
AcpiOsVprintf(const char *Format, va_list Args)
{
	static char line[256];
	static size_t len;
	char buf[256];
	int n = vsnprintf(buf, sizeof(buf), Format, Args);

	for (int i = 0; i < n && i < (int)sizeof(buf) - 1; i++) {
		if (buf[i] == '\n' || len == sizeof(line) - 1) {
			line[len] = '\0';
			IOLog("%s\n", line);	/* ACPICA prefixes its own */
			len = 0;
			if (buf[i] == '\n')
				continue;
		}
		line[len++] = buf[i];
	}
}

ACPI_STATUS
AcpiOsSignal(UINT32 Function, void *Info)
{
	if (Function == ACPI_SIGNAL_FATAL) {
		ACPI_SIGNAL_FATAL_INFO *f = (ACPI_SIGNAL_FATAL_INFO *)Info;

		IOLog("ACPI: fatal error from the firmware: type 0x%x code 0x%x arg 0x%x\n",
		    f->Type, f->Code, f->Argument);
	}
	return AE_OK;
}

ACPI_STATUS
AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
	(void)SleepState;
	(void)RegaValue;
	(void)RegbValue;
	return AE_OK;
}
