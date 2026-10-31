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
 * configd: when it has a network service for our interface (PreferencesMonitor
 * publishes Setup:/Network/Service/<id>/Interface), the worker publishes
 * what it configured there, as IPConfiguration does on macOS:
 *
 *	State:/Network/Service/<id>/IPv4	Addresses, SubnetMasks, Router,
 *						InterfaceName
 *	State:/Network/Service/<id>/IPv6	Addresses, PrefixLength, Router,
 *						InterfaceName
 *	State:/Network/Service/<id>/DNS		ServerAddresses, DomainName
 *	State:/Network/Service/<id>/DHCP	Option_<n>, LeaseStartTime
 *
 * and IPMonitor chooses the primary service, adds the default route and
 * writes resolv.conf (telling mDNSResponder). Without configd (or before it
 * has the service) the worker does those itself (iface.c).
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>

#include <ifaddrs.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "dhcpclient.h"

static SCDynamicStoreRef	store;
static CFStringRef		service_id;	/* NULL: no service (yet) */

/* The ID of the service whose Interface is ours; NULL if none. */
static CFStringRef
copy_service_id(void)
{
	CFStringRef pattern, ifname;
	CFArrayRef patterns;
	CFDictionaryRef dict;
	CFStringRef found = NULL;
	CFIndex n;

	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
	    kSCDynamicStoreDomainSetup, kSCCompAnyRegex, kSCEntNetInterface);
	patterns = CFArrayCreate(NULL, (const void **)&pattern, 1, &kCFTypeArrayCallBacks);
	dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
	CFRelease(patterns);
	CFRelease(pattern);
	if (dict == NULL)
		return NULL;
	ifname = CFStringCreateWithCString(NULL, ifs.name, kCFStringEncodingASCII);
	n = CFDictionaryGetCount(dict);
	{
		const void *keys[n > 0 ? n : 1], *values[n > 0 ? n : 1];

		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (CFIndex i = 0; i < n && found == NULL; i++) {
			CFDictionaryRef iface = values[i];
			CFStringRef dev;
			CFArrayRef parts;

			if (CFGetTypeID(iface) != CFDictionaryGetTypeID())
				continue;
			dev = CFDictionaryGetValue(iface, kSCPropNetInterfaceDeviceName);
			if (dev == NULL || !CFEqual(dev, ifname))
				continue;
			/* Setup:/Network/Service/<id>/Interface */
			parts = CFStringCreateArrayBySeparatingStrings(NULL, keys[i], CFSTR("/"));
			if (CFArrayGetCount(parts) == 5)
				found = CFRetain(CFArrayGetValueAtIndex(parts, 3));
			CFRelease(parts);
		}
	}
	CFRelease(ifname);
	CFRelease(dict);
	return found;
}

bool
store_active(void)
{
	if (service_id != NULL)
		return true;
	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL, CFSTR("dhcpclient"), NULL, NULL);
		if (store == NULL)
			return false;	/* configd is not running */
	}
	service_id = copy_service_id();
	if (service_id != NULL) {
		char id[128];

		CFStringGetCString(service_id, id, sizeof(id), kCFStringEncodingUTF8);
		logmsg(LOG_NOTICE, "publishing to configd, service %s", id);
	}
	return service_id != NULL;
}

static bool
string_in(CFTypeRef v, int af, void *out)
{
	char buf[INET6_ADDRSTRLEN];

	if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID())
		v = CFArrayGetCount(v) > 0 ? CFArrayGetValueAtIndex(v, 0) : NULL;
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID() ||
	    !CFStringGetCString(v, buf, sizeof(buf), kCFStringEncodingASCII))
		return false;
	return inet_pton(af, buf, out) == 1;
}

static CFDictionaryRef
copy_setup(CFStringRef entity)
{
	CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
	    kSCDynamicStoreDomainSetup, service_id, entity);
	CFPropertyListRef v = SCDynamicStoreCopyValue(store, key);

	CFRelease(key);
	if (v != NULL && CFGetTypeID(v) != CFDictionaryGetTypeID()) {
		CFRelease(v);
		v = NULL;
	}
	return v;
}

/*
 * The service's IPv4 and IPv6 in Setup:. A protocol without its entity is
 * off; a method this worker does not know (PPP's and the like) leaves the
 * call failing, and the worker as it is.
 */
bool
store_setup(struct setup *s)
{
	CFDictionaryRef v4, v6;
	CFStringRef m;
	bool ok = true;

	if (service_id == NULL)
		return false;
	memset(s, 0, sizeof(*s));
	v4 = copy_setup(kSCEntNetIPv4);
	v6 = copy_setup(kSCEntNetIPv6);
	m = v4 ? CFDictionaryGetValue(v4, kSCPropNetIPv4ConfigMethod) : NULL;
	if (v4 == NULL)
		s->method4 = M4_NONE;
	else if (m == NULL || CFEqual(m, kSCValNetIPv4ConfigMethodDHCP))
		s->method4 = M4_DHCP;
	else if (CFEqual(m, kSCValNetIPv4ConfigMethodBOOTP))
		s->method4 = M4_BOOTP;
	else if (CFEqual(m, kSCValNetIPv4ConfigMethodManual) ||
	    CFEqual(m, kSCValNetIPv4ConfigMethodINFORM)) {
		s->method4 = CFEqual(m, kSCValNetIPv4ConfigMethodManual) ?
		    M4_MANUAL : M4_INFORM;
		if (!string_in(CFDictionaryGetValue(v4, kSCPropNetIPv4Addresses),
		    AF_INET, &s->addr4))
			ok = false;
		if (!string_in(CFDictionaryGetValue(v4, kSCPropNetIPv4SubnetMasks),
		    AF_INET, &s->mask4))
			s->mask4.s_addr = htonl(0xffffff00);
		string_in(CFDictionaryGetValue(v4, kSCPropNetIPv4Router), AF_INET,
		    &s->router4);
	} else
		ok = false;
	m = v6 ? CFDictionaryGetValue(v6, kSCPropNetIPv6ConfigMethod) : NULL;
	if (v6 == NULL)
		s->method6 = M6_NONE;
	else if (m == NULL || CFEqual(m, kSCValNetIPv6ConfigMethodAutomatic) ||
	    CFEqual(m, kSCValNetIPv6ConfigMethodRouterAdvertisement))
		s->method6 = M6_AUTOMATIC;
	else if (CFEqual(m, kSCValNetIPv6ConfigMethodLinkLocal))
		s->method6 = M6_LINKLOCAL;
	else if (CFEqual(m, kSCValNetIPv6ConfigMethodManual)) {
		CFTypeRef p = CFDictionaryGetValue(v6, kSCPropNetIPv6PrefixLength);

		s->method6 = M6_MANUAL;
		s->prefix6 = 64;
		if (p != NULL && CFGetTypeID(p) == CFArrayGetTypeID() &&
		    CFArrayGetCount(p) > 0)
			p = CFArrayGetValueAtIndex(p, 0);
		if (p != NULL && CFGetTypeID(p) == CFNumberGetTypeID())
			CFNumberGetValue(p, kCFNumberIntType, &s->prefix6);
		if (!string_in(CFDictionaryGetValue(v6, kSCPropNetIPv6Addresses),
		    AF_INET6, &s->addr6))
			ok = false;
	} else
		ok = false;
	if (v4 != NULL)
		CFRelease(v4);
	if (v6 != NULL)
		CFRelease(v6);
	return ok;
}

static CFStringRef
copy_string4(struct in_addr a)
{
	return CFStringCreateWithCString(NULL, inet_ntoa(a), kCFStringEncodingASCII);
}

static CFStringRef
copy_string6(const struct in6_addr *a)
{
	char buf[INET6_ADDRSTRLEN];
	struct in6_addr addr = *a;

	if (IN6_IS_ADDR_LINKLOCAL(&addr))
		addr.s6_addr[2] = addr.s6_addr[3] = 0;	/* the kernel's scope */
	inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
	return CFStringCreateWithCString(NULL, buf, kCFStringEncodingASCII);
}

static void
add_value(CFMutableDictionaryRef dict, CFStringRef key, CFTypeRef value)
{
	CFDictionarySetValue(dict, key, value);
	CFRelease(value);
}

static void
append_value(CFMutableArrayRef array, CFTypeRef value)
{
	CFArrayAppendValue(array, value);
	CFRelease(value);
}

static CFMutableDictionaryRef
new_dict(void)
{
	return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
}

static CFMutableArrayRef
new_array(void)
{
	return CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
}

static CFStringRef
copy_ifname(void)
{
	return CFStringCreateWithCString(NULL, ifs.name, kCFStringEncodingASCII);
}

/* The DHCP options of the lease, as SCDynamicStoreCopyDHCPInfo returns them. */
static CFDictionaryRef
copy_dhcp(const struct lease *l)
{
	CFMutableDictionaryRef dict = new_dict();
	char key[16];

#define OPTION(n, bytes, len) do {					\
	snprintf(key, sizeof(key), "Option_%d", (n));			\
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingASCII); \
	add_value(dict, k, CFDataCreate(NULL, (const UInt8 *)(bytes), (CFIndex)(len))); \
	CFRelease(k);							\
} while (0)
	OPTION(1, &l->mask, 4);
	if (l->router.s_addr != 0)
		OPTION(3, &l->router, 4);
	if (l->ndns > 0)
		OPTION(6, l->dns, 4 * l->ndns);
	if (l->domain[0] != '\0')
		OPTION(15, l->domain, strlen(l->domain));
	if (l->lease_time != 0) {
		uint32_t t = htonl(l->lease_time);

		OPTION(51, &t, 4);
	}
	if (l->server.s_addr != 0)
		OPTION(54, &l->server, 4);
#undef OPTION
	add_value(dict, CFSTR("LeaseStartTime"),
	    CFDateCreate(NULL, CFAbsoluteTimeGetCurrent() - (double)(now() - l->start)));
	return dict;
}

/* Sets (value non-NULL) or removes State:/Network/Service/<id>/<entity>. */
static void
set_entity(CFStringRef entity, CFDictionaryRef value)
{
	CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
	    kSCDynamicStoreDomainState, service_id, entity);

	if (value != NULL) {
		SCDynamicStoreSetValue(store, key, value);
		CFRelease(value);
	} else {
		SCDynamicStoreRemoveValue(store, key);
	}
	CFRelease(key);
}

bool
store_publish(void)
{
	CFMutableDictionaryRef ipv4 = NULL, ipv6 = NULL, dns = NULL;
	CFMutableArrayRef servers = new_array();
	const struct lease *l = &ifs.current;
	struct ifaddrs *list, *ifa;
	const char *domain;

	if (!store_active()) {
		CFRelease(servers);
		return false;
	}

	if (l->addr.s_addr != 0) {
		CFMutableArrayRef a = new_array(), m = new_array();

		ipv4 = new_dict();
		append_value(a, copy_string4(l->addr));
		append_value(m, copy_string4(l->mask));
		add_value(ipv4, kSCPropNetIPv4Addresses, a);
		add_value(ipv4, kSCPropNetIPv4SubnetMasks, m);
		if (l->router.s_addr != 0)
			add_value(ipv4, kSCPropNetIPv4Router, copy_string4(l->router));
		add_value(ipv4, kSCPropInterfaceName, copy_ifname());
		for (int i = 0; i < l->ndns; i++)
			append_value(servers, copy_string4(l->dns[i]));
	}

	/* The interface's IPv6 addresses (SLAAC, DHCPv6, manual), not link-local. */
	if (getifaddrs(&list) == 0) {
		CFMutableArrayRef a = new_array(), p = new_array();

		for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
			const struct sockaddr_in6 *sin6, *mask;
			int plen = 0;

			if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET6 ||
			    strcmp(ifa->ifa_name, ifs.name) != 0)
				continue;
			sin6 = (const struct sockaddr_in6 *)(const void *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				continue;
			mask = (const struct sockaddr_in6 *)(const void *)ifa->ifa_netmask;
			for (int i = 0; mask != NULL && i < 16; i++)
				plen += __builtin_popcount(mask->sin6_addr.s6_addr[i]);
			append_value(a, copy_string6(&sin6->sin6_addr));
			append_value(p, CFNumberCreate(NULL, kCFNumberIntType, &plen));
		}
		freeifaddrs(list);
		if (CFArrayGetCount(a) > 0) {
			ipv6 = new_dict();
			add_value(ipv6, kSCPropNetIPv6Addresses, a);
			add_value(ipv6, kSCPropNetIPv6PrefixLength, p);
			if (!IN6_IS_ADDR_UNSPECIFIED(&ifs.router6))
				add_value(ipv6, kSCPropNetIPv6Router, copy_string6(&ifs.router6));
			add_value(ipv6, kSCPropInterfaceName, copy_ifname());
		} else {
			CFRelease(a);
			CFRelease(p);
		}
	}

	for (int i = 0; i < ifs.ndns6; i++)
		append_value(servers, copy_string6(&ifs.dns6[i]));
	for (int i = 0; i < ifs.nrdnss; i++) {
		bool dup = false;

		for (int j = 0; j < ifs.ndns6; j++)
			dup |= memcmp(&ifs.dns6[j], &ifs.rdnss[i], sizeof(struct in6_addr)) == 0;
		if (!dup)
			append_value(servers, copy_string6(&ifs.rdnss[i]));
	}
	domain = l->domain[0] != '\0' ? l->domain : ifs.domain6;
	if (CFArrayGetCount(servers) > 0) {
		dns = new_dict();
		CFDictionarySetValue(dns, kSCPropNetDNSServerAddresses, servers);
		if (domain[0] != '\0')
			add_value(dns, kSCPropNetDNSDomainName,
			    CFStringCreateWithCString(NULL, domain, kCFStringEncodingUTF8));
	}
	CFRelease(servers);

	set_entity(kSCEntNetIPv4, ipv4);
	set_entity(kSCEntNetIPv6, ipv6);
	set_entity(kSCEntNetDNS, dns);
	set_entity(kSCEntNetDHCP, l->addr.s_addr != 0 && l->lease_time != 0 ? copy_dhcp(l) : NULL);
	return true;
}
