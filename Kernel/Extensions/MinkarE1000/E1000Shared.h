/*-
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
 * MinkarE1000's way into Intel's shared e1000 code (e1000/), for the chips
 * whose setup it does not do itself: the PCH's I217-I219 (and the 82577-
 * 82579 before them) and the I210/I211. Opaque, so that the driver's own
 * register names and the shared code's do not meet.
 */
#ifndef _MINKAR_E1000_SHARED_H_
#define _MINKAR_E1000_SHARED_H_

#include <IOKit/pci/IOPCIDevice.h>

enum E1000Family {
	kE1000FamilyOther,	/* 8254x, 82571-82574, ...: the driver's own setup */
	kE1000FamilyPCH,	/* ICH/PCH LAN: 82577-82579, I217, I218, I219 */
	kE1000FamilyIGB,	/* I210, I211 */
};

struct E1000SharedChip;

/* Identify the chip from its PCI IDs; NULL if the shared code does not know it. */
E1000SharedChip *E1000SharedCreate(IOPCIDevice *pci, volatile uint8_t *regs);
void E1000SharedFree(E1000SharedChip *chip);
E1000Family E1000SharedFamily(const E1000SharedChip *chip);
const char *E1000SharedName(const E1000SharedChip *chip);
/* PCH LAN before Sunrise Point reads its NVM flash through BAR 1. */
bool E1000SharedNeedsFlashBAR(const E1000SharedChip *chip);
void E1000SharedSetFlash(E1000SharedChip *chip, volatile uint8_t *flash);
/* Reset, read the MAC address, set up the PHY and start autonegotiation. */
bool E1000SharedInit(E1000SharedChip *chip, uint8_t mac[6]);
/* Stop the chip (reset), as before a power-down. */
void E1000SharedReset(E1000SharedChip *chip);
/* Link state; mbps 10, 100 or 1000. */
void E1000SharedLink(E1000SharedChip *chip, bool *up, uint32_t *mbps, bool *full);

#endif /* _MINKAR_E1000_SHARED_H_ */
