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
 * The ACPI interpreter (ACPICA) in IOACPIFamily: its OS services
 * (ACPIOSLayer.cpp) and what the platform expert asks of it
 * (ACPIInterpreter.cpp).
 */

#ifndef _ACPIOSLAYER_H
#define _ACPIOSLAYER_H

#include <IOKit/IOService.h>
#include <stdint.h>

/* Interrupt specifier flags, as AppleAPIC reads them (PICShared.h). */
enum {
	kACPIInterruptLevel	= 0x1,	/* else edge */
	kACPIInterruptActiveLow	= 0x2,	/* else active high */
};

/* ACPIOSLayer.cpp */
void ACPIOSLayerSetRootPointer(uint64_t rsdp);

/* ACPIInterpreter.cpp */
void ACPIOSLayerSetInterruptController(const OSSymbol *name);
bool ACPIInterpreterStart(void);
bool ACPIInterpreterPowerOff(void);
bool ACPIInterpreterResolvePCIInterrupt(IOService *bridge, uint32_t device, uint32_t pin,
    uint32_t *gsi, uint32_t *flags);
bool ACPIInterpreterInterruptFlags(uint32_t gsi, uint32_t *flags);
void ACPIOSLayerSCIRoute(uint32_t irq, uint32_t *gsi, uint32_t *flags);
IOService *ACPIOSLayerCreateInterruptNub(const char *name, uint32_t gsi, uint32_t flags);

#endif /* !_ACPIOSLAYER_H */
