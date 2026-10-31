/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * This file was modified by William Kent in 2017 to support the PureDarwin
 * project. This notice is included in support of clause 2.2(b) of the License.
 */

#include "ACPICPU.h"
#include "ACPIPlatformExpert.h"

#undef super
#define super IOCPU

uint32_t ncpus = 0;

OSDefineMetaClassAndStructors(ACPICPU, IOCPU);

/*
 * Minkar: SMP. The kernel brings a processor up itself (INIT and SIPI,
 * intel_startCPU), once the platform has told it of every processor: each
 * one is registered with ml_processor_register, and when all are, each is
 * started, the boot processor first, as macOS's ACPI platform does. One
 * CPU interrupt controller for them all, made with their number, which
 * sets the kernel's maximum (ml_init_max_cpus). Each processor enables its
 * interrupts in initCPU, which the kernel calls on it as it comes up.
 */
#define kACPICPUMax	64		/* the kernel's MAX_CPUS on x86_64 */

/* Exported (Unsupported), declared in the kernel's i386/lapic.h. */
extern "C" uint32_t ml_get_apicid(uint32_t cpu);

static IOLock *gACPICPULock;
static ACPICPUInterruptController *gACPICPUIC;
static ACPICPU *gACPICPUs[kACPICPUMax];
static uint32_t gACPICPUExpected, gACPICPURegistered;
static bool gACPICPUStarted;

/* The nub is the ACPICPU itself, made with the MADT's properties. */
static uint32_t
numberProperty(IOCPU *cpu, const char *key, uint32_t dflt)
{
	const OSSymbol *sym = OSSymbol::withCString(key);
	OSNumber *n = OSDynamicCast(OSNumber, cpu->getProperty(sym));

	sym->release();
	return n ? n->unsigned32BitValue() : dflt;
}

IOService *ACPICPU::probe(IOService *provider, SInt32 *score) {
	return this;
}

bool ACPICPU::startCommon() {
	bool all;

	if (startCommonCompleted) return true;

	if (gACPICPULock == NULL) {
		IOLock *lock = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, lock, &gACPICPULock))
			IOLockFree(lock);
	}
	IOLockLock(gACPICPULock);
	if (gACPICPUExpected == 0) {
		gACPICPUExpected = ncpus < kACPICPUMax ? ncpus : kACPICPUMax;
		if (gACPICPUExpected == 0)
			gACPICPUExpected = 1;
	}
	bootCPU = (apicID == ml_get_apicid(0));
	setCPUState(kIOCPUStateUninitalized);
	if (gACPICPUs[index] == NULL) {
		gACPICPUs[index] = this;
		gACPICPURegistered++;
	}
	all = !gACPICPUStarted && gACPICPURegistered >= gACPICPUExpected;
	if (all)
		gACPICPUStarted = true;
	IOLockUnlock(gACPICPULock);

	registerService();
	startCommonCompleted = true;
	if (all)
		startAll();
	return true;
}

/*
 * The CPU interrupt controller, for n processors. Making it sets the
 * kernel's number of processors (ml_init_max_cpus), which the commpage
 * waits for: by then every processor has to be registered, as the
 * commpage takes the count of logical processors then (libmalloc makes
 * one magazine for each, and picks one by the processor it runs on).
 */
static bool
makeInterruptController(ACPICPU *cpu, uint32_t n)
{
	gACPICPUIC = new ACPICPUInterruptController;
	if (gACPICPUIC == NULL ||
	    gACPICPUIC->initCPUInterruptController(n) != kIOReturnSuccess) {
		IOLog("ACPICPU: no CPU interrupt controller\n");
		return false;
	}
	gACPICPUIC->attach(cpu);
	gACPICPUIC->registerCPUInterruptController();
	return true;
}

/*
 * Every processor is here: register them with the kernel, the boot
 * processor first (it takes that), set the number of processors, then
 * start them in the same order.
 */
void ACPICPU::startAll(void) {
	ACPICPU *boot = NULL;
	uint32_t i, n = 1, started = 0;

	for (i = 0; i < kACPICPUMax; i++)
		if (gACPICPUs[i] != NULL && gACPICPUs[i]->bootCPU)
			boot = gACPICPUs[i];
	if (boot != NULL && ml_processor_register((cpu_id_t)boot,
	    boot->apicID, &boot->machProcessor, TRUE, FALSE) != KERN_SUCCESS) {
		IOLog("ACPICPU: the boot processor not registered\n");
		boot = NULL;
	}
	if (boot == NULL) {
		/*
		 * The boot processor is not among them: register none (the
		 * kernel would panic), and keep running on it alone.
		 */
		IOLog("ACPICPU: running on the boot processor (APIC %u) "
		    "alone\n", ml_get_apicid(0));
		for (i = 0; i < kACPICPUMax; i++)
			if (gACPICPUs[i] != NULL) {
				if (makeInterruptController(gACPICPUs[i], 1)) {
					gACPICPUs[i]->cpuIC = gACPICPUIC;
					gACPICPUs[i]->initCPU(true);
				}
				break;
			}
		return;
	}
	for (i = 0; i < kACPICPUMax; i++) {
		ACPICPU *cpu = gACPICPUs[i];

		if (cpu == NULL || cpu == boot)
			continue;
		if (ml_processor_register((cpu_id_t)cpu, cpu->apicID,
		    &cpu->machProcessor, FALSE, FALSE) != KERN_SUCCESS) {
			/* Left out: the others still start without it. */
			IOLog("ACPICPU: cpu%u (APIC %u) not registered\n", i,
			    cpu->apicID);
			gACPICPUs[i] = NULL;
		} else
			n++;
	}
	if (!makeInterruptController(boot, n))
		return;
	for (i = 0; i < kACPICPUMax; i++)
		if (gACPICPUs[i] != NULL)
			gACPICPUs[i]->cpuIC = gACPICPUIC;

	ml_processor_register((cpu_id_t)boot, boot->apicID,
	    &boot->machProcessor, TRUE, TRUE);
	for (i = 0; i < kACPICPUMax; i++) {
		ACPICPU *cpu = gACPICPUs[i];

		if (cpu == NULL || cpu == boot)
			continue;
		if (ml_processor_register((cpu_id_t)cpu, cpu->apicID,
		    &cpu->machProcessor, FALSE, TRUE) == KERN_SUCCESS)
			started++;
	}
	IOLog("ACPICPU: %u processor(s) started besides the boot one\n",
	    started);
}

bool ACPICPU::start(IOService *provider) {
	index = numberProperty(this, "cpu-number", 0);
	apicID = numberProperty(this, "apic-id", 0);
	IOLog("ACPICPU::start: cpu%u, APIC %u\n", index, apicID);
	if (index >= kACPICPUMax) {
		IOLog("ACPICPU: cpu%u is beyond the %u the kernel takes\n",
		    index, kACPICPUMax);
		return false;
	}

	if (!super::start(provider)) return false;
	setCPUNumber(index);
	return startCommon();
}

void ACPICPU::initCPU(bool boot) {
	if (!interruptsEnabled) {
		cpuIC->enableCPUInterrupt(this);
		interruptsEnabled = true;
	}
	setCPUState(kIOCPUStateRunning);
}

void ACPICPU::quiesceCPU() {
	// Not required.
}

/* On x86 the kernel starts a processor itself (intel_startCPU). */
kern_return_t ACPICPU::startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr) {
	return KERN_FAILURE;
}

void ACPICPU::haltCPU() {
	// Not required.
}

const OSSymbol *ACPICPU::getCPUName() {
        char buf[32];
        snprintf(buf, sizeof(buf), "cpu%u", index);
	return OSSymbol::withCString(buf);
}

#pragma mark -
#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(ACPICPUInterruptController, IOCPUInterruptController);

IOReturn ACPICPUInterruptController::handleInterrupt(void *refCon,
        IOService *nub, int source) {
    return ((ACPIPlatformExpert *)getPlatform())->handleInterrupt(refCon, nub, source);
}
