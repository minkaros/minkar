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

/* Display modes and configuration over the window server's interface. */

#include <stdlib.h>
#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

int
SLSCopyDisplayModes(SLSDisplayID display, uint32_t max, SLSDisplayMode *modes,
    uint32_t *count)
{
	SLSDisplayMode list[kSLSMaxDisplayModes];
	mach_msg_type_number_t n = kSLSMaxDisplayModes;
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (count == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_modes(server, display, &result, list, &n) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (result != kSLSErrorSuccess)
		return result;
	if (modes == NULL) {
		*count = n;
		return kSLSErrorSuccess;
	}
	if (n > max)
		n = max;
	memcpy(modes, list, n * sizeof(SLSDisplayMode));
	*count = n;
	return kSLSErrorSuccess;
}

int
SLSGetCurrentDisplayMode(SLSDisplayID display, SLSDisplayMode *mode)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (mode == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_current_mode(server, display, &result, mode) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

struct SLSDisplayConfiguration {
	uint32_t		count;
	SLSDisplayConfigOp	ops[kSLSMaxDisplayConfigOps];
};

int
SLSBeginDisplayConfiguration(SLSDisplayConfigRef *config)
{
	if (config == NULL)
		return kSLSErrorIllegalArgument;
	*config = calloc(1, sizeof(struct SLSDisplayConfiguration));
	return *config != NULL ? kSLSErrorSuccess : kSLSErrorFailure;
}

static int
addOp(SLSDisplayConfigRef config, const SLSDisplayConfigOp *op)
{
	if (config == NULL)
		return kSLSErrorIllegalArgument;
	if (config->count == kSLSMaxDisplayConfigOps)
		return kSLSErrorRangeCheck;
	config->ops[config->count++] = *op;
	return kSLSErrorSuccess;
}

int
SLSConfigureDisplayWithDisplayMode(SLSDisplayConfigRef config, SLSDisplayID display,
    uint32_t modeID)
{
	SLSDisplayConfigOp op = { .kind = kSLSConfigOpMode, .display = display,
	    .modeID = modeID };

	return addOp(config, &op);
}

int
SLSConfigureDisplayOrigin(SLSDisplayConfigRef config, SLSDisplayID display,
    int32_t x, int32_t y)
{
	SLSDisplayConfigOp op = { .kind = kSLSConfigOpOrigin, .display = display,
	    .x = x, .y = y };

	return addOp(config, &op);
}

int
SLSConfigureDisplayMirrorOfDisplay(SLSDisplayConfigRef config, SLSDisplayID display,
    SLSDisplayID primary)
{
	SLSDisplayConfigOp op = { .kind = kSLSConfigOpMirror, .display = display,
	    .mirrorOf = primary };

	return addOp(config, &op);
}

int
SLSCompleteDisplayConfiguration(SLSDisplayConfigRef config, uint32_t option)
{
	int32_t result = kSLSErrorFailure, failed = -1;
	mach_port_t server = SLSServerPort();

	if (config == NULL)
		return kSLSErrorIllegalArgument;
	if (server != MACH_PORT_NULL &&
	    ws_display_configure(server, option, config->ops, config->count, &result,
	    &failed) != KERN_SUCCESS)
		result = kSLSErrorFailure;
	free(config);
	return result;
}

int
SLSCancelDisplayConfiguration(SLSDisplayConfigRef config)
{
	if (config == NULL)
		return kSLSErrorIllegalArgument;
	free(config);
	return kSLSErrorSuccess;
}
