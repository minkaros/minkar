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
 * kextstart: tells IOKit that the kext daemon has finished starting.
 *
 * XNU keeps the IOService root busy at boot until kextd sends
 * kIOCatalogKextdFinishedLaunching; until then IOKit never becomes quiet
 * (IOKitWaitQuiet, the busy state configd's InterfaceNamer waits on).
 * ravynOS has no kextd: every kext is in the kernelcache, so there is
 * nothing to send but that message. launchd runs this once at boot.
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>

#include <stdio.h>
#include <sysexits.h>

int
main(void)
{
	mach_port_t master = MACH_PORT_NULL;
	kern_return_t kr;

	kr = IOMasterPort(bootstrap_port, &master);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "kextstart: IOMasterPort: 0x%x\n", kr);
		return EX_OSERR;
	}
	/*
	 * Not kIOCatalogKextdActive: the kernel would then ask kextd to load
	 * kexts, and drop the startup kexts it has not loaded.
	 */
	kr = IOCatalogueSendData(master, kIOCatalogKextdFinishedLaunching, NULL, 0);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "kextstart: IOCatalogueSendData: 0x%x\n", kr);
		return EX_OSERR;
	}
	return 0;
}
