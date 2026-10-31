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
 * The power button. ACPI tells of a press with its fixed event (PM1's
 * PWRBTN_STS) or, where the FADT says the button is a device, with a
 * notify (0x80) to the PNP0C0C device. Both come in the SCI's primary
 * interrupt context: they queue a thread call, which tells the button
 * service's clients (powerd) and counts the presses.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread_call.h>

extern "C" {
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
}

#include "ACPIPowerButton.h"

#define super IOService
OSDefineMetaClassAndStructors(ACPIPowerButton, IOService);

static ACPIPowerButton *gButton;
static thread_call_t gButtonCall;

void
ACPIPowerButton::pressedCall(void *, void *)
{
	if (gButton == NULL)
		return;
	gButton->presses++;
	gButton->setProperty("PowerButtonPresses", gButton->presses, 32);
	IOLog("ACPI: power button pressed\n");
	gButton->messageClients(kACPIPowerButtonPressed);
}

/* The fixed event: primary interrupt context. */
static UINT32
fixedEvent(void *)
{
	thread_call_enter(gButtonCall);
	return ACPI_INTERRUPT_HANDLED;
}

/* A PNP0C0C device's notify: 0x80 is a press. */
static void
deviceNotify(ACPI_HANDLE, UINT32 value, void *)
{
	if (value == 0x80)
		thread_call_enter(gButtonCall);
}

static ACPI_STATUS
foundDevice(ACPI_HANDLE handle, UINT32, void *, void **)
{
	if (ACPI_SUCCESS(AcpiInstallNotifyHandler(handle, ACPI_DEVICE_NOTIFY,
	    deviceNotify, NULL)))
		IOLog("ACPI: power button device (PNP0C0C)\n");
	return AE_OK;
}

void
ACPIPowerButton::install(IOService *provider)
{
	ACPIPowerButton *b;
	ACPI_STATUS st;

	if (gButton != NULL || provider == NULL)
		return;
	gButtonCall = thread_call_allocate(pressedCall, NULL);
	b = new ACPIPowerButton;
	if (gButtonCall == NULL || b == NULL || !b->init()) {
		if (b != NULL)
			b->release();
		return;
	}
	b->setName("PowerButton");
	if (!b->attach(provider) || !b->start(provider)) {
		b->release();
		return;
	}
	gButton = b;
	b->registerService();
	b->release();

	/* The FADT says whether the button is the fixed one or a device. */
	if (!(AcpiGbl_FADT.Flags & ACPI_FADT_POWER_BUTTON)) {
		st = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
		    fixedEvent, NULL);
		if (ACPI_SUCCESS(st))
			IOLog("ACPI: power button (fixed event)\n");
		else
			IOLog("ACPI: power button fixed event: %s\n",
			    AcpiFormatException(st));
	}
	AcpiGetDevices((char *)"PNP0C0C", foundDevice, NULL, NULL);
}
