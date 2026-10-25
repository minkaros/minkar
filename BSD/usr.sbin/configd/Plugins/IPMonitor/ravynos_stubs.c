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
 * IPMonitor's parts that Minkar does not build, as no-ops:
 *
 * - the configuration agents (agent-monitor.m and friends), which publish
 *   DNS and proxy settings as NetworkExtension network agents;
 * - NAT64 prefix discovery (nat64-configuration.c, Network.framework);
 * - the SMB server preferences (smb-configuration.c, the SMB framework).
 *
 * The rest of IPMonitor (routes, primary service, DNS, proxies, the
 * network information) works without them.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <uuid/uuid.h>

#include "agent-monitor.h"
#include "nat64-configuration.h"
#include "smb-configuration.h"

void
process_AgentMonitor(void)
{
}

void
process_AgentMonitor_DNS(void)
{
}

void
process_AgentMonitor_Proxy(void)
{
}

const void *
copy_proxy_information_for_agent_uuid(uuid_t uuid, uint64_t *length)
{
#pragma unused(uuid)
	*length = 0;
	return NULL;
}

const void *
copy_dns_information_for_agent_uuid(uuid_t uuid, uint64_t *length)
{
#pragma unused(uuid)
	*length = 0;
	return NULL;
}

Boolean
is_nat64_prefix_request(CFStringRef change, CFStringRef *interface)
{
#pragma unused(change, interface)
	return FALSE;
}

void
nat64_prefix_request_add_pattern(CFMutableArrayRef patterns)
{
#pragma unused(patterns)
}

void
nat64_configuration_update(CFSetRef updates, CFSetRef requests, CFSetRef cancellations)
{
#pragma unused(updates, requests, cancellations)
}

void
load_smb_configuration(Boolean verbose)
{
#pragma unused(verbose)
}
