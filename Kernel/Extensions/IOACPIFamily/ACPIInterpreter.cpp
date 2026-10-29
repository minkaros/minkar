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
#include <mach/mach_time.h>
#include <pexpert/pexpert.h>

/* osfmk machine_routines.h does not build as C++ here. */
extern "C" boolean_t ml_set_interrupts_enabled(boolean_t enable);

extern "C" {
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
}

#include "ACPIOSLayer.h"

static const OSSymbol *gInterruptController;
static IOLock *gStartLock;
static volatile bool gStartDone;
static bool gStarted;
static ACPI_HANDLE gRootBridge;
static uint64_t gStartDeadline;		/* how long anyone waits for the start */
static const char * volatile gStage = "not started";

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

	/* gStage names each step, for the watchdog's report if one hangs. */
	if ((gStage = "AcpiInitializeSubsystem",
	    ACPI_FAILURE(st = AcpiInitializeSubsystem())))
		IOLog("ACPI: AcpiInitializeSubsystem: %s\n", AcpiFormatException(st));
	else if ((gStage = "AcpiInitializeTables",
	    ACPI_FAILURE(st = AcpiInitializeTables(NULL, 16, FALSE))))
		IOLog("ACPI: AcpiInitializeTables: %s\n", AcpiFormatException(st));
	else if ((gStage = "AcpiLoadTables (the DSDT and SSDTs)",
	    ACPI_FAILURE(st = AcpiLoadTables())))
		IOLog("ACPI: AcpiLoadTables: %s\n", AcpiFormatException(st));
	else if ((gStage = "AcpiEnableSubsystem (ACPI mode, SCI)",
	    ACPI_FAILURE(st = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))))
		IOLog("ACPI: AcpiEnableSubsystem: %s\n", AcpiFormatException(st));
	else if ((gStage = "AcpiInitializeObjects (_STA, _INI)",
	    ACPI_FAILURE(st = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))))
		IOLog("ACPI: AcpiInitializeObjects: %s\n", AcpiFormatException(st));
	else
		ok = true;
	gStage = ok ? "running" : "failed";

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

/* The AML method the interpreter runs now, as \_SB_.PCI0._INI. */
static void
currentMethod(char *buf, size_t len)
{
	ACPI_THREAD_STATE *t = AcpiGbl_CurrentWalkList;
	ACPI_WALK_STATE *w = t != NULL ? t->WalkStateList : NULL;
	ACPI_NAMESPACE_NODE *n = w != NULL ? w->MethodNode : NULL;
	char names[8][5];
	int depth = 0;

	for (; n != NULL && n->Parent != NULL && depth < 8; n = n->Parent) {
		memcpy(names[depth], n->Name.Ascii, 4);
		names[depth++][4] = 0;
	}
	buf[0] = 0;
	if (depth == 0) {
		strlcpy(buf, "none", len);
		return;
	}
	while (depth-- > 0) {
		strlcat(buf, "\\", len);
		strlcat(buf, names[depth], len);
	}
}

/*
 * If the start hangs (seen on a PC), says every 10 seconds where: the
 * step, the AML method and the interpreter's last access to the hardware.
 */
static void
watchThread(void *, wait_result_t)
{
	for (int secs = 10; secs <= 60; secs += 10) {
		IOSleep(10000);
		if (gStartDone)
			return;
		char method[64];
		currentMethod(method, sizeof(method));
		IOLog("ACPI: still starting after %ds: %s, method %s, last %s 0x%llx = 0x%llx (%llu)\n",
		    secs, gStage, method,
		    gACPIOSLast.what != NULL ? gACPIOSLast.what : "none",
		    gACPIOSLast.address, gACPIOSLast.value, gACPIOSLast.count);
	}
}

bool
ACPIInterpreterStart(void)
{
	thread_t thread;

	if (gStartLock != NULL)
		return true;
	if ((gStartLock = IOLockAlloc()) == NULL)
		return false;
	clock_interval_to_deadline(30, kSecondScale, &gStartDeadline);
	if (kernel_thread_start(startThread, NULL, &thread) != KERN_SUCCESS) {
		gStartDone = true;
		return false;
	}
	thread_deallocate(thread);
	if (kernel_thread_start(watchThread, NULL, &thread) == KERN_SUCCESS)
		thread_deallocate(thread);
	return true;
}

/* Waits (a while) for the interpreter; false if it is not running. */
static bool
waitStarted(void)
{
	if (gStartLock == NULL)
		return false;
	/* One deadline for every caller: a start that hangs costs 30 seconds
	 * once, not once per PCI device. */
	IOLockLock(gStartLock);
	while (!gStartDone && mach_absolute_time() < gStartDeadline) {
		if (IOLockSleepDeadline(gStartLock, (event_t)&gStartDone, gStartDeadline,
		    THREAD_UNINT) == THREAD_TIMED_OUT)
			break;
	}
	IOLockUnlock(gStartLock);
	return gStarted;
}

/* ---- Power ------------------------------------------------------------- */

/*
 * Turns the machine off (ACPI S5), as halting does on macOS: _PTS, then
 * SLP_TYP/SLP_EN with interrupts off. Returns only if that failed.
 */
/*
 * The value a Name(_S5_, Package() {a, b, ...}) gives SLP_TYPa and SLP_TYPb,
 * found in the DSDT's bytes, for when the interpreter is not running.
 */
static bool
findS5(const uint8_t *aml, size_t len, uint8_t *typa, uint8_t *typb)
{
	for (size_t i = 0; i + 8 < len; i++) {
		if (memcmp(aml + i, "_S5_", 4) != 0 || aml[i + 4] != 0x12)	/* PackageOp */
			continue;
		size_t p = i + 5;
		p += 1 + (aml[p] >> 6);		/* PkgLength: lead byte and its extra bytes */
		p++;				/* NumElements */
		uint8_t v[2];
		for (int k = 0; k < 2 && p < len; k++) {
			if (aml[p] == 0x0A && p + 1 < len) {	/* BytePrefix */
				v[k] = aml[p + 1];
				p += 2;
			} else if (aml[p] <= 0x01 || aml[p] == 0xFF) {	/* Zero, One, Ones */
				v[k] = aml[p] == 0xFF ? 0x7 : aml[p];
				p++;
			} else {
				return false;
			}
		}
		*typa = v[0];
		*typb = v[1];
		return true;
	}
	return false;
}

static inline uint16_t inw16(uint16_t port)
{
	uint16_t v;
	__asm__ __volatile__("inw %w1, %w0" : "=a"(v) : "Nd"(port));
	return v;
}

static inline void outw16(uint16_t port, uint16_t v)
{
	__asm__ __volatile__("outw %w0, %w1" : : "a"(v), "Nd"(port));
}

/*
 * S5 without the interpreter: SLP_TYP from the DSDT's _S5_ and SLP_EN
 * written to the FADT's PM1 control registers (no _PTS).
 */
static bool
powerOffDirect(void)
{
	ACPI_TABLE_HEADER *t, *dsdt;
	ACPI_TABLE_FADT *fadt;
	uint8_t typa, typb;

	if (ACPI_FAILURE(AcpiGetTable((char *)ACPI_SIG_FADT, 1, &t)) ||
	    ACPI_FAILURE(AcpiGetTable((char *)ACPI_SIG_DSDT, 1, &dsdt))) {
		IOLog("ACPI: no FADT or DSDT, cannot power off\n");
		return false;
	}
	fadt = (ACPI_TABLE_FADT *)t;
	if (!findS5((const uint8_t *)dsdt + sizeof(*dsdt),
	    dsdt->Length - sizeof(*dsdt), &typa, &typb)) {
		IOLog("ACPI: no _S5_ in the DSDT, cannot power off\n");
		return false;
	}
	uint16_t a = (uint16_t)fadt->Pm1aControlBlock;
	uint16_t b = (uint16_t)fadt->Pm1bControlBlock;
	uint16_t evt = (uint16_t)fadt->Pm1aEventBlock;
	IOLog("ACPI: S5 without the interpreter: SLP_TYP %u/%u, PM1a_CNT 0x%x\n",
	    typa, typb, a);
	if (a == 0)
		return false;
	(void)ml_set_interrupts_enabled(FALSE);
	if (evt != 0)
		outw16(evt, 0xFFFF);		/* clear the wake status bits */
	outw16(a, (uint16_t)((inw16(a) & ~(0x7 << 10)) | (typa << 10)));
	if (b != 0)
		outw16(b, (uint16_t)((inw16(b) & ~(0x7 << 10)) | (typb << 10)));
	outw16(a, (uint16_t)(inw16(a) | (1 << 13)));		/* SLP_EN */
	if (b != 0)
		outw16(b, (uint16_t)(inw16(b) | (1 << 13)));
	IODelay(100000);
	return false;
}

bool
ACPIInterpreterPowerOff(void)
{
	ACPI_STATUS st;
	boolean_t istate;

	/* acpi_direct_s5=1 takes the path without the interpreter (tests). */
	uint32_t direct = 0;
	(void)PE_parse_boot_argn("acpi_direct_s5", &direct, sizeof(direct));
	if (!gStarted || direct) {
		IOLog("ACPI: interpreter not running (%s)\n", direct ? "acpi_direct_s5" : gStage);
		return powerOffDirect();
	}
	/* Each step is shown, to see on a PC where powering off stops. */
	IOLog("ACPI: S5: _PTS\n");
	if (ACPI_FAILURE(st = AcpiEnterSleepStatePrep(ACPI_STATE_S5))) {
		IOLog("ACPI: S5 prepare: %s\n", AcpiFormatException(st));
		return false;
	}
	IOLog("ACPI: S5: SLP_TYP, SLP_EN\n");
	istate = ml_set_interrupts_enabled(FALSE);
	st = AcpiEnterSleepState(ACPI_STATE_S5);
	ml_set_interrupts_enabled(istate);
	IOLog("ACPI: S5: %s\n", AcpiFormatException(st));
	return false;
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
