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
 * kextstart: tells IOKit that the kext daemon has finished starting.
 *
 * XNU keeps the IOService root busy at boot until kextd sends
 * kIOCatalogKextdFinishedLaunching; until then IOKit never becomes quiet
 * (IOKitWaitQuiet, the busy state configd's InterfaceNamer waits on).
 * Minkar has no kextd: every kext is in the kernelcache, so there is
 * nothing to send but that message. launchd runs this once at boot.
 *
 * It then waits for IOKit to become quiet (drivers matched and started)
 * and for every PCI network device to have its interface attached and its
 * link state known (both are reported after IOKit gets quiet), and asks
 * launchd to start the console
 * getty (KeepAlive/AfterInitialDemand: it waits for this first start), so
 * the drivers' kernel messages come before the login prompt, not over it.
 * From then on the kernel's messages stay off the console screen
 * (kern.console_quiet); they still reach the log.
 *
 * If IOKit does not get quiet in time, it names the services still busy,
 * to find which driver holds it up on a new machine.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOBSD.h>

#include <sys/sysctl.h>

#include <errno.h>
#include <stdbool.h>
#include <launch.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#define CONSOLE_GETTY	"org.minkar.getty.console"
#define QUIET_TIMEOUT	30	/* seconds */
#define NET_TIMEOUT	10	/* seconds, for the network interfaces */

/* kIOLinkStatus bits (IONetworkMedium.h). */
#define LINK_STATUS_KEY	"IOLinkStatus"
#define LINK_VALID	0x1

static bool
number_property(io_registry_entry_t e, const char *key, uint32_t *value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFTypeRef v = IORegistryEntryCreateCFProperty(e, k, NULL, 0);
	bool ok = v != NULL && CFGetTypeID(v) == CFNumberGetTypeID() &&
	    CFNumberGetValue(v, kCFNumberSInt32Type, value);

	if (v != NULL)
		CFRelease(v);
	CFRelease(k);
	return ok;
}

/* True if the controller has an attached interface with a BSD name and
 * reports a known link state. */
static bool
controller_ready(io_service_t controller)
{
	io_iterator_t children;
	io_registry_entry_t child;
	uint32_t link = 0;
	bool named = false;

	if (IORegistryEntryGetChildIterator(controller, kIOServicePlane,
	    &children) != KERN_SUCCESS)
		return false;
	while ((child = IOIteratorNext(children)) != 0) {
		if (IOObjectConformsTo(child, "IONetworkInterface")) {
			CFTypeRef name = IORegistryEntryCreateCFProperty(child,
			    CFSTR(kIOBSDNameKey), NULL, 0);
			if (name != NULL) {
				named = true;
				CFRelease(name);
			}
		}
		IOObjectRelease(child);
	}
	IOObjectRelease(children);
	return named && number_property(controller, LINK_STATUS_KEY, &link) &&
	    (link & LINK_VALID) != 0;
}

/* Whether a network controller driver is attached to the PCI device. */
static bool
has_network_driver(io_service_t pci)
{
	io_iterator_t children;
	io_registry_entry_t c;
	bool found = false;

	if (IORegistryEntryGetChildIterator(pci, kIOServicePlane,
	    &children) != KERN_SUCCESS)
		return false;
	while ((c = IOIteratorNext(children)) != 0) {
		if (IOObjectConformsTo(c, "IONetworkController"))
			found = true;
		IOObjectRelease(c);
	}
	IOObjectRelease(children);
	return found;
}

/* Names the busy services whose children are all idle: the ones holding
 * IOKit up. */
static void
report_busy(mach_port_t master)
{
	io_iterator_t it;
	io_registry_entry_t e;
	int shown = 0;

	if (IORegistryCreateIterator(master, kIOServicePlane,
	    kIORegistryIterateRecursively, &it) != KERN_SUCCESS)
		return;
	while ((e = IOIteratorNext(it)) != 0 && shown < 10) {
		uint32_t busy = 0;
		io_iterator_t children;
		io_registry_entry_t c;
		bool busy_child = false;
		io_name_t name;

		if (IOServiceGetBusyState(e, &busy) != KERN_SUCCESS || busy == 0) {
			IOObjectRelease(e);
			continue;
		}
		if (IORegistryEntryGetChildIterator(e, kIOServicePlane,
		    &children) == KERN_SUCCESS) {
			while ((c = IOIteratorNext(children)) != 0) {
				uint32_t cb = 0;
				if (IOServiceGetBusyState(c, &cb) == KERN_SUCCESS && cb != 0)
					busy_child = true;
				IOObjectRelease(c);
			}
			IOObjectRelease(children);
		}
		if (!busy_child && IORegistryEntryGetName(e, name) == KERN_SUCCESS) {
			io_name_t cls = "";
			IOObjectGetClass(e, cls);
			fprintf(stderr, "kextstart: still busy: %s (%s)\n", name, cls);
			shown++;
		}
		IOObjectRelease(e);
	}
	IOObjectRelease(it);
}

/* PCI network devices (class code 0x02xxxx) that have a driver: only those
 * can be waited for (a Wi-Fi card with no driver never gets ready). */
static int
pci_network_devices(mach_port_t master)
{
	io_iterator_t it;
	io_service_t svc;
	int n = 0;

	if (IOServiceGetMatchingServices(master, IOServiceMatching("IOPCIDevice"),
	    &it) != KERN_SUCCESS)
		return 0;
	while ((svc = IOIteratorNext(it)) != 0) {
		CFTypeRef cc = IORegistryEntryCreateCFProperty(svc, CFSTR("class-code"),
		    NULL, 0);
		if (cc != NULL && CFGetTypeID(cc) == CFDataGetTypeID() &&
		    CFDataGetLength(cc) >= 3 && CFDataGetBytePtr(cc)[2] == 0x02 &&
		    has_network_driver(svc))
			n++;
		if (cc != NULL)
			CFRelease(cc);
		IOObjectRelease(svc);
	}
	IOObjectRelease(it);
	return n;
}

/* Network controllers with an attached, named interface and a known link
 * state. */
static int
network_ready(mach_port_t master)
{
	io_iterator_t it;
	io_service_t svc;
	int n = 0;

	if (IOServiceGetMatchingServices(master,
	    IOServiceMatching("IONetworkController"), &it) != KERN_SUCCESS)
		return 0;
	while ((svc = IOIteratorNext(it)) != 0) {
		if (controller_ready(svc))
			n++;
		IOObjectRelease(svc);
	}
	IOObjectRelease(it);
	return n;
}

static void
wait_for_network(mach_port_t master)
{
	int expected = pci_network_devices(master), ready = 0;

	for (int tenths = 0; tenths < NET_TIMEOUT * 10; tenths++) {
		if ((ready = network_ready(master)) >= expected)
			return;
		usleep(100000);
	}
	fprintf(stderr, "kextstart: %d of %d network interfaces ready after %ds\n",
	    ready, expected, NET_TIMEOUT);
}

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

	mach_timespec_t timeout = { QUIET_TIMEOUT, 0 };
	kr = IOKitWaitQuiet(master, &timeout);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "kextstart: IOKitWaitQuiet: 0x%x\n", kr);
		report_busy(master);
	}
	wait_for_network(master);

	/* The login is next: the kernel keeps its messages off the screen. */
	int quiet = 1;
	if (sysctlbyname("kern.console_quiet", NULL, NULL, &quiet,
	    sizeof(quiet)) != 0)
		fprintf(stderr, "kextstart: kern.console_quiet: %s\n",
		    strerror(errno));
	/* Whether or not IOKit got quiet, the console must come up. The getty
	 * job may not be loaded yet (launchctl loads the jobs as we run). */
	int error = 0;
	for (int tries = 0; tries < 50; tries++) {
		launch_data_t msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
		launch_data_dict_insert(msg, launch_data_new_string(CONSOLE_GETTY),
		    LAUNCH_KEY_STARTJOB);
		launch_data_t resp = launch_msg(msg);
		launch_data_free(msg);
		if (resp == NULL) {
			error = errno;
		} else {
			error = launch_data_get_type(resp) == LAUNCH_DATA_ERRNO ?
			    launch_data_get_errno(resp) : 0;
			launch_data_free(resp);
		}
		if (error != ESRCH)
			break;
		usleep(200000);
	}
	if (error != 0) {
		fprintf(stderr, "kextstart: start %s: %s\n", CONSOLE_GETTY,
		    strerror(error));
		return EX_OSERR;
	}
	return 0;
}
