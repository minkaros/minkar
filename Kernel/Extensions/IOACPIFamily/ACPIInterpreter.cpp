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
 * ACPIInterpreter: what IOACPIFamily asks of ACPICA. It starts the
 * interpreter (tables, namespace, ACPI mode, SCI) on its own thread once
 * the IOAPIC is registered, tells the firmware the APIC interrupt model
 * (\_PIC(1)), and resolves PCI INTx lines through _PRT and the PCI
 * interrupt link devices, the way macOS's ACPI platform does.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOInterrupts.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <kern/thread.h>

extern "C" {
#include <contrib/dev/acpica/include/acpi.h>
}

#include "ACPIOSLayer.h"

static const OSSymbol *gInterruptController;
static IOLock *gStartLock;
static volatile bool gStartDone;
static bool gStarted;
static ACPI_HANDLE gRootBridge;

/* What _PRT and the link devices said about each GSI. */
#define MAX_GSI 256
static uint8_t gGSIFlags[MAX_GSI];
static bool gGSIKnown[MAX_GSI];

void
ACPIOSLayerSetInterruptController(const OSSymbol *name)
{
	if (name != NULL)
		name->retain();
	gInterruptController = name;
}

static ACPI_STATUS
findRootBridge(ACPI_HANDLE object, UINT32, void *, void **ret)
{
	*(ACPI_HANDLE *)ret = object;
	return AE_CTRL_TERMINATE;
}

static void
startThread(void *, wait_result_t)
{
	ACPI_OBJECT arg;
	ACPI_OBJECT_LIST args = { 1, &arg };
	ACPI_STATUS st;
	bool ok = false;

	/* The SCI handler needs the IOAPIC's interrupt controller. */
	if (gInterruptController != NULL)
		IOService::getPlatform()->lookUpInterruptController(
		    (OSSymbol *)gInterruptController);

	if (ACPI_FAILURE(st = AcpiInitializeSubsystem()))
		IOLog("ACPI: AcpiInitializeSubsystem: %s\n", AcpiFormatException(st));
	else if (ACPI_FAILURE(st = AcpiInitializeTables(NULL, 16, FALSE)))
		IOLog("ACPI: AcpiInitializeTables: %s\n", AcpiFormatException(st));
	else if (ACPI_FAILURE(st = AcpiLoadTables()))
		IOLog("ACPI: AcpiLoadTables: %s\n", AcpiFormatException(st));
	else if (ACPI_FAILURE(st = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION)))
		IOLog("ACPI: AcpiEnableSubsystem: %s\n", AcpiFormatException(st));
	else if (ACPI_FAILURE(st = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION)))
		IOLog("ACPI: AcpiInitializeObjects: %s\n", AcpiFormatException(st));
	else
		ok = true;

	if (ok) {
		/* APIC mode: _PRT then names the IOAPIC routing. */
		arg.Type = ACPI_TYPE_INTEGER;
		arg.Integer.Value = 1;
		st = AcpiEvaluateObject(NULL, (ACPI_STRING)"\\_PIC", &args, NULL);
		if (ACPI_FAILURE(st) && st != AE_NOT_FOUND)
			IOLog("ACPI: \\_PIC(1): %s\n", AcpiFormatException(st));

		AcpiGetDevices((char *)"PNP0A08", findRootBridge, NULL, &gRootBridge);
		if (gRootBridge == NULL)
			AcpiGetDevices((char *)"PNP0A03", findRootBridge, NULL, &gRootBridge);
		IOLog("ACPI: ACPICA %08x running; PCI root bridge %s\n",
		    ACPI_CA_VERSION, gRootBridge != NULL ? "found" : "not found");
	}

	IOLockLock(gStartLock);
	gStarted = ok;
	gStartDone = true;
	IOLockWakeup(gStartLock, (event_t)&gStartDone, false);
	IOLockUnlock(gStartLock);
}

bool
ACPIInterpreterStart(void)
{
	thread_t thread;

	if (gStartLock != NULL)
		return true;
	if ((gStartLock = IOLockAlloc()) == NULL)
		return false;
	if (kernel_thread_start(startThread, NULL, &thread) != KERN_SUCCESS) {
		gStartDone = true;
		return false;
	}
	thread_deallocate(thread);
	return true;
}

/* Waits (a while) for the interpreter; false if it is not running. */
static bool
waitStarted(void)
{
	uint64_t deadline;

	if (gStartLock == NULL)
		return false;
	clock_interval_to_deadline(30, kSecondScale, &deadline);
	IOLockLock(gStartLock);
	while (!gStartDone) {
		if (IOLockSleepDeadline(gStartLock, (event_t)&gStartDone, deadline,
		    THREAD_UNINT) == THREAD_TIMED_OUT)
			break;
	}
	IOLockUnlock(gStartLock);
	return gStarted;
}

/* ---- PCI interrupt routing --------------------------------------------- */

struct LinkIRQ {
	uint32_t	gsi;
	uint32_t	flags;
	bool		found;
};

static ACPI_STATUS
linkResource(ACPI_RESOURCE *res, void *context)
{
	struct LinkIRQ *irq = (struct LinkIRQ *)context;
	uint8_t triggering, polarity;

	if (irq->found)
		return AE_OK;
	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		if (res->Data.Irq.InterruptCount == 0)
			return AE_OK;
		irq->gsi = res->Data.Irq.Interrupts[0];
		triggering = res->Data.Irq.Triggering;
		polarity = res->Data.Irq.Polarity;
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (res->Data.ExtendedIrq.InterruptCount == 0)
			return AE_OK;
		irq->gsi = res->Data.ExtendedIrq.Interrupts[0];
		triggering = res->Data.ExtendedIrq.Triggering;
		polarity = res->Data.ExtendedIrq.Polarity;
		break;
	default:
		return AE_OK;
	}
	irq->flags = (triggering == ACPI_LEVEL_SENSITIVE ? kACPIInterruptLevel : 0) |
	    (polarity == ACPI_ACTIVE_LOW ? kACPIInterruptActiveLow : 0);
	irq->found = irq->gsi != 0;
	return AE_OK;
}

/* The GSI of (device, pin) on the root bus, from the root bridge's _PRT. */
static bool
routeOnRootBus(uint32_t device, uint32_t pin, uint32_t *gsi, uint32_t *flags)
{
	ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_PCI_ROUTING_TABLE *prt;
	bool found = false;
	ACPI_STATUS st;

	if (gRootBridge == NULL)
		return false;
	if (ACPI_FAILURE(st = AcpiGetIrqRoutingTable(gRootBridge, &buf))) {
		IOLog("ACPI: _PRT: %s\n", AcpiFormatException(st));
		return false;
	}
	for (prt = (ACPI_PCI_ROUTING_TABLE *)buf.Pointer; prt->Length != 0;
	    prt = (ACPI_PCI_ROUTING_TABLE *)((uint8_t *)prt + prt->Length)) {
		if (((prt->Address >> 16) & 0xFFFF) != device || prt->Pin != pin)
			continue;
		if (prt->Source[0] == '\0') {
			/* A GSI directly. */
			*gsi = prt->SourceIndex;
			*flags = kACPIInterruptLevel | kACPIInterruptActiveLow;
			found = true;
		} else {
			ACPI_HANDLE link;
			struct LinkIRQ irq = { 0, 0, false };

			if (ACPI_SUCCESS(AcpiGetHandle(gRootBridge, prt->Source, &link)))
				AcpiWalkResources(link, (char *)METHOD_NAME__CRS, linkResource, &irq);
			if (irq.found) {
				*gsi = irq.gsi;
				*flags = irq.flags;
				found = true;
			} else {
				IOLog("ACPI: link %s for device %u pin %c has no IRQ\n",
				    prt->Source, device, 'A' + pin);
			}
		}
		break;
	}
	ACPI_FREE(buf.Pointer);
	return found;
}

/*
 * (device, pin) on the bus of bridge. Behind PCI-to-PCI bridges the pin
 * is swizzled up to the root bus (PCI-to-PCI Bridge spec 9.1).
 */
bool
ACPIInterpreterResolvePCIInterrupt(IOService *bridge, uint32_t device, uint32_t pin,
    uint32_t *gsi, uint32_t *flags)
{
	IOPCIDevice *pdev;

	if (!waitStarted() || pin > 3)
		return false;
	while ((pdev = OSDynamicCast(IOPCIDevice, bridge)) != NULL) {
		IORegistryEntry *parentBridge;

		pin = (pin + device) % 4;
		device = pdev->getDeviceNumber();
		parentBridge = pdev->getParentEntry(gIOServicePlane);
		bridge = parentBridge != NULL ?
		    OSDynamicCast(IOService, parentBridge->getParentEntry(gIOServicePlane)) : NULL;
	}
	if (!routeOnRootBus(device, pin, gsi, flags))
		return false;
	if (*gsi < MAX_GSI) {
		gGSIFlags[*gsi] = (uint8_t)*flags;
		gGSIKnown[*gsi] = true;
	}
	return true;
}

bool
ACPIInterpreterInterruptFlags(uint32_t gsi, uint32_t *flags)
{
	if (gsi >= MAX_GSI || !gGSIKnown[gsi])
		return false;
	*flags = gGSIFlags[gsi];
	return true;
}

/* ---- The SCI ------------------------------------------------------------ */

/*
 * The SCI's GSI and trigger: an interrupt source override in the MADT if
 * there is one, else the IRQ as a GSI, level-triggered and active low
 * (ACPI 6.5, 5.2.9: the SCI is sharable, level, active low).
 */
void
ACPIOSLayerSCIRoute(uint32_t irq, uint32_t *gsi, uint32_t *flags)
{
	ACPI_TABLE_HEADER *table;

	*gsi = irq;
	*flags = kACPIInterruptLevel | kACPIInterruptActiveLow;
	if (ACPI_FAILURE(AcpiGetTable((char *)ACPI_SIG_MADT, 1, &table)))
		return;
	uint8_t *p = (uint8_t *)table + sizeof(ACPI_TABLE_MADT);
	uint8_t *end = (uint8_t *)table + table->Length;

	while (p + sizeof(ACPI_SUBTABLE_HEADER) <= end) {
		ACPI_SUBTABLE_HEADER *sub = (ACPI_SUBTABLE_HEADER *)p;

		if (sub->Length == 0)
			break;
		if (sub->Type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE) {
			ACPI_MADT_INTERRUPT_OVERRIDE *iso = (ACPI_MADT_INTERRUPT_OVERRIDE *)sub;

			if (iso->SourceIrq == irq) {
				uint16_t pol = iso->IntiFlags & ACPI_MADT_POLARITY_MASK;
				uint16_t trig = (iso->IntiFlags & ACPI_MADT_TRIGGER_MASK) >> 2;

				*gsi = iso->GlobalIrq;
				*flags = 0;
				/* "Conforms" keeps the SCI's own: level, active low. */
				if (trig != ACPI_MADT_TRIGGER_EDGE)
					*flags |= kACPIInterruptLevel;
				if (pol != ACPI_MADT_POLARITY_ACTIVE_HIGH)
					*flags |= kACPIInterruptActiveLow;
				break;
			}
		}
		p += sub->Length;
	}
}

/* A nub whose interrupt 0 is gsi on the IOAPIC, with these trigger flags. */
IOService *
ACPIOSLayerCreateInterruptNub(const char *name, uint32_t gsi, uint32_t flags)
{
	IOService *nub = new IOService;
	OSArray *controllers, *specifiers;
	uint32_t spec[2] = { gsi, flags };
	OSData *data;

	if (nub == NULL || !nub->init() || gInterruptController == NULL) {
		if (nub != NULL)
			nub->release();
		return NULL;
	}
	nub->setName(name);
	controllers = OSArray::withObjects((const OSObject **)&gInterruptController, 1);
	data = OSData::withBytes(spec, sizeof(spec));
	specifiers = data != NULL ? OSArray::withObjects((const OSObject **)&data, 1) : NULL;
	if (controllers == NULL || specifiers == NULL) {
		OSSafeReleaseNULL(controllers);
		OSSafeReleaseNULL(specifiers);
		OSSafeReleaseNULL(data);
		nub->release();
		return NULL;
	}
	nub->setProperty(gIOInterruptControllersKey, controllers);
	nub->setProperty(gIOInterruptSpecifiersKey, specifiers);
	controllers->release();
	specifiers->release();
	data->release();
	nub->attach(IOService::getPlatform());
	return nub;
}
