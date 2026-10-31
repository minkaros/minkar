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

#ifndef _ACPIPOWERBUTTON_H
#define _ACPIPOWERBUTTON_H

#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>

/*
 * The power button: ACPI's fixed event, or a PNP0C0C device's notify.
 * The service it publishes (class ACPIPowerButton) sends its interested
 * clients this message at each press; powerd asks for an orderly
 * shutdown then (or sleep, where the machine can and it is set).
 */
#define kACPIPowerButtonPressed	iokit_vendor_specific_msg(0x0100)

class ACPIPowerButton : public IOService {
	OSDeclareDefaultStructors(ACPIPowerButton);

public:
	/* Once ACPICA runs: the service, and the handlers. */
	static void install(IOService *provider);

private:
	static void pressedCall(void *, void *);
	uint32_t	presses;
};

#endif /* !_ACPIPOWERBUTTON_H */
