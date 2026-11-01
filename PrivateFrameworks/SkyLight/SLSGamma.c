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

/* Display transfer (gamma) tables over the window server's interface. */

#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

int
SLSGetGammaTableCapacity(SLSDisplayID display, uint32_t *capacity)
{
	SLSDisplayInfo info;
	int err;

	if (capacity == NULL)
		return kSLSErrorIllegalArgument;
	if ((err = SLSGetDisplayInfo(display, &info)) != kSLSErrorSuccess)
		return err;
	*capacity = kSLSGammaTableCapacity;
	return kSLSErrorSuccess;
}

int
SLSSetDisplayTransferByTable(SLSDisplayID display, uint32_t count, const float *red,
    const float *green, const float *blue)
{
	ws_gamma_table_t table;
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (count < 2 || count > kSLSGammaTableCapacity || red == NULL || green == NULL ||
	    blue == NULL)
		return kSLSErrorIllegalArgument;
	memcpy(table, red, count * sizeof(float));
	memcpy(table + count, green, count * sizeof(float));
	memcpy(table + 2 * count, blue, count * sizeof(float));
	if (server == MACH_PORT_NULL || ws_display_set_transfer_table(server, display, count,
	    table, 3 * count, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetDisplayTransferByTable(SLSDisplayID display, uint32_t capacity, float *red,
    float *green, float *blue, uint32_t *count)
{
	ws_gamma_table_t table;
	mach_msg_type_number_t n = 3 * kSLSGammaTableCapacity;
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();
	uint32_t entries;

	if (count == NULL || (capacity != 0 && (red == NULL || green == NULL || blue == NULL)))
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_transfer_table(server, display, &result, table, &n) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (result != kSLSErrorSuccess)
		return result;
	entries = n / 3;
	if (entries > capacity)
		entries = capacity;
	if (entries != 0) {
		memcpy(red, table, entries * sizeof(float));
		memcpy(green, table + n / 3, entries * sizeof(float));
		memcpy(blue, table + 2 * (n / 3), entries * sizeof(float));
	}
	*count = entries;
	return kSLSErrorSuccess;
}

int
SLSSetDisplayTransferByFormula(SLSDisplayID display, const SLSGammaFormula *formula)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (formula == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_display_set_transfer_formula(server, display,
	    *formula, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetDisplayTransferByFormula(SLSDisplayID display, SLSGammaFormula *formula)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (formula == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_transfer_formula(server, display, &result, formula) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSRestoreDisplayTransfers(void)
{
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_restore_transfers(server) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return kSLSErrorSuccess;
}
