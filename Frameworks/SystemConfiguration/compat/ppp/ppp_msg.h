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
 * The PPP, IPSec and VPN connection states PPPController publishes, which
 * IPMonitor reads. ravynOS has no PPPController: no service has them, so
 * the values only need to be distinct.
 */

#ifndef _RAVYNOS_COMPAT_PPP_MSG_H
#define _RAVYNOS_COMPAT_PPP_MSG_H

enum {
	PPP_IDLE		= 0,
	PPP_INITIALIZE		= 1,
	PPP_CONNECTLINK		= 2,
	PPP_STATERESERVED	= 3,
	PPP_ESTABLISH		= 4,
	PPP_AUTHENTICATE	= 5,
	PPP_CALLBACK		= 6,
	PPP_NETWORK		= 7,
	PPP_RUNNING		= 8,
	PPP_TERMINATE		= 9,
	PPP_DISCONNECTLINK	= 10,
	PPP_HOLDOFF		= 11,
	PPP_ONHOLD		= 12,
	PPP_WAITONBUSY		= 13,
};

/* IPSec and VPN service states (kSCPropNetIPSecStatus, kSCPropNetVPNStatus). */
enum {
	IPSEC_IDLE		= 0,
	IPSEC_INITIALIZE	= 1,
	IPSEC_CONTACT		= 2,
	IPSEC_PHASE1		= 3,
	IPSEC_PHASE1AUTH	= 4,
	IPSEC_PHASE2		= 5,
	IPSEC_RUNNING		= 6,
	IPSEC_TERMINATE		= 7,
};

enum {
	VPN_IDLE		= 0,
	VPN_STARTING		= 1,
	VPN_RUNNING		= 2,
	VPN_STOPPING		= 3,
	VPN_LOADING		= 4,
	VPN_LOADED		= 5,
	VPN_UNLOADING		= 6,
};

#endif /* !_RAVYNOS_COMPAT_PPP_MSG_H */
