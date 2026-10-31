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
 * networksetup: the network settings of System Preferences from the
 * command line, through SystemConfiguration's network configuration API.
 * Services (by their names) are listed, ordered, created, removed,
 * renamed, enabled and disabled; each one's IPv4 (manual, DHCP, BOOTP,
 * manual with a DHCP router), IPv6, DNS servers, search domains and
 * proxies are read and set; hardware ports, MAC addresses, MTU and media
 * are those of the interfaces; locations are the configuration's sets.
 * The addresses a service has now come from configd's dynamic store.
 * There is no Wi-Fi yet: the AirPort commands say so.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* SystemConfiguration's (private on macOS too): adds a service for each
 * interface the set has none for. */
Boolean SCNetworkSetEstablishDefaultConfiguration(SCNetworkSetRef);

static SCPreferencesRef prefs;
static bool changed;

/* ---- Strings ------------------------------------------------------------- */

static CFStringRef
cfstr(const char *s)
{
	return (CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8));
}

static const char *
cstr(CFStringRef s, char *buf, size_t len)
{
	buf[0] = '\0';
	if (s != NULL && CFGetTypeID(s) == CFStringGetTypeID())
		CFStringGetCString(s, buf, (CFIndex)len, kCFStringEncodingUTF8);
	return (buf);
}

static bool
cfeq(CFStringRef s, const char *c)
{
	char buf[256];

	return (s != NULL && strcmp(cstr(s, buf, sizeof(buf)), c) == 0);
}

static CFArrayRef
strings_array(char **v, int n)
{
	CFMutableArrayRef a = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	int i;

	for (i = 0; i < n; i++) {
		CFStringRef s = cfstr(v[i]);

		CFArrayAppendValue(a, s);
		CFRelease(s);
	}
	return (a);
}

static int
invalid(void)
{
	printf("** Error: The parameters were not valid.\n");
	return (1);
}

/* ---- The configuration ---------------------------------------------------- */

static void
open_prefs(void)
{
	prefs = SCPreferencesCreate(NULL, CFSTR("networksetup"), NULL);
	if (prefs == NULL)
		errx(1, "the network configuration is not available");
}

static int
save(void)
{
	if (!changed)
		return (0);
	if (!SCPreferencesCommitChanges(prefs) ||
	    !SCPreferencesApplyChanges(prefs)) {
		printf("** Error: Unable to commit changes to network database: "
		    "%s.\n", SCErrorString(SCError()));
		return (1);
	}
	return (0);
}

static SCNetworkSetRef
current_set(void)
{
	SCNetworkSetRef set = SCNetworkSetCopyCurrent(prefs);

	if (set == NULL)
		errx(1, "there is no current location");
	return (set);
}

/* The current location's services, in its service order. */
static CFArrayRef
services(void)
{
	SCNetworkSetRef set = current_set();
	CFArrayRef all = SCNetworkSetCopyServices(set);
	CFArrayRef order = SCNetworkSetGetServiceOrder(set);
	CFMutableArrayRef sorted = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	CFIndex i, j, n = all ? CFArrayGetCount(all) : 0;

	for (i = 0; order != NULL && i < CFArrayGetCount(order); i++)
		for (j = 0; j < n; j++) {
			SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, j);

			if (CFEqual(SCNetworkServiceGetServiceID(s),
			    CFArrayGetValueAtIndex(order, i)))
				CFArrayAppendValue(sorted, s);
		}
	for (j = 0; j < n; j++) {
		SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, j);

		if (!CFArrayContainsValue(sorted,
		    CFRangeMake(0, CFArrayGetCount(sorted)), s))
			CFArrayAppendValue(sorted, s);
	}
	if (all != NULL)
		CFRelease(all);
	CFRelease(set);
	return (sorted);
}

static SCNetworkServiceRef
find_service(const char *name)
{
	CFArrayRef all = services();
	SCNetworkServiceRef found = NULL;
	CFIndex i;

	for (i = 0; name != NULL && i < CFArrayGetCount(all); i++) {
		SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, i);

		if (cfeq(SCNetworkServiceGetName(s), name)) {
			found = (SCNetworkServiceRef)CFRetain(s);
			break;
		}
	}
	CFRelease(all);
	if (found == NULL)
		printf("%s is not a recognized network service.\n",
		    name ? name : "");
	return (found);
}

/* An interface by its BSD name or its hardware port's name. */
static SCNetworkInterfaceRef
find_interface(const char *name)
{
	CFArrayRef all = SCNetworkInterfaceCopyAll();
	SCNetworkInterfaceRef found = NULL;
	CFIndex i;

	for (i = 0; all != NULL && i < CFArrayGetCount(all); i++) {
		SCNetworkInterfaceRef ifp = CFArrayGetValueAtIndex(all, i);

		if (cfeq(SCNetworkInterfaceGetBSDName(ifp), name) ||
		    cfeq(SCNetworkInterfaceGetLocalizedDisplayName(ifp), name)) {
			found = (SCNetworkInterfaceRef)CFRetain(ifp);
			break;
		}
	}
	if (all != NULL)
		CFRelease(all);
	return (found);
}

static CFDictionaryRef
protocol_config(SCNetworkServiceRef s, CFStringRef type)
{
	SCNetworkProtocolRef p = SCNetworkServiceCopyProtocol(s, type);
	CFDictionaryRef c = NULL;

	if (p != NULL) {
		c = SCNetworkProtocolGetConfiguration(p);
		if (c != NULL)
			CFRetain(c);
		CFRelease(p);
	}
	return (c);
}

static bool
protocol_enabled(SCNetworkServiceRef s, CFStringRef type)
{
	SCNetworkProtocolRef p = SCNetworkServiceCopyProtocol(s, type);
	bool on = false;

	if (p != NULL) {
		on = SCNetworkProtocolGetEnabled(p);
		CFRelease(p);
	}
	return (on);
}

/* Replaces a protocol's configuration (adding the protocol if needed). */
static int
set_protocol(SCNetworkServiceRef s, CFStringRef type, CFDictionaryRef config,
    bool enable)
{
	SCNetworkProtocolRef p = SCNetworkServiceCopyProtocol(s, type);

	if (p == NULL) {
		if (!SCNetworkServiceAddProtocolType(s, type) ||
		    (p = SCNetworkServiceCopyProtocol(s, type)) == NULL)
			return (invalid());
	}
	if ((config != NULL && !SCNetworkProtocolSetConfiguration(p, config)) ||
	    !SCNetworkProtocolSetEnabled(p, enable)) {
		CFRelease(p);
		return (invalid());
	}
	CFRelease(p);
	changed = true;
	return (0);
}

/* A protocol's configuration with one key set (or removed when NULL). */
static int
set_protocol_key(SCNetworkServiceRef s, CFStringRef type, CFStringRef key,
    CFTypeRef value)
{
	CFDictionaryRef old = protocol_config(s, type);
	CFMutableDictionaryRef c = old != NULL ?
	    CFDictionaryCreateMutableCopy(NULL, 0, old) :
	    CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	int rc;

	if (value != NULL)
		CFDictionarySetValue(c, key, value);
	else
		CFDictionaryRemoveValue(c, key);
	rc = set_protocol(s, type, c, true);
	CFRelease(c);
	if (old != NULL)
		CFRelease(old);
	return (rc);
}

static CFTypeRef
dict_get(CFDictionaryRef d, CFStringRef key)
{
	return (d != NULL ? CFDictionaryGetValue(d, key) : NULL);
}

static long
dict_long(CFDictionaryRef d, CFStringRef key)
{
	CFTypeRef v = dict_get(d, key);
	long n = 0;

	if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
		CFNumberGetValue(v, kCFNumberLongType, &n);
	return (n);
}

static const char *
first_string(CFTypeRef v, char *buf, size_t len)
{
	buf[0] = '\0';
	if (v != NULL && CFGetTypeID(v) == CFArrayGetTypeID() &&
	    CFArrayGetCount(v) > 0)
		v = CFArrayGetValueAtIndex(v, 0);
	return (cstr(v, buf, len));
}

static void
print_strings(CFTypeRef v)
{
	char buf[512];
	CFIndex i;

	for (i = 0; v != NULL && i < CFArrayGetCount(v); i++)
		printf("%s\n", cstr(CFArrayGetValueAtIndex(v, i), buf,
		    sizeof(buf)));
}

/* ---- Services ------------------------------------------------------------- */

static int
list_services(void)
{
	CFArrayRef all = services();
	char buf[256];
	CFIndex i;

	printf("An asterisk (*) denotes that a network service is disabled.\n");
	for (i = 0; i < CFArrayGetCount(all); i++) {
		SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, i);

		printf("%s%s\n", SCNetworkServiceGetEnabled(s) ? "" : "*",
		    cstr(SCNetworkServiceGetName(s), buf, sizeof(buf)));
	}
	CFRelease(all);
	return (0);
}

static int
list_service_order(void)
{
	CFArrayRef all = services();
	char name[256], port[256], dev[64];
	CFIndex i;

	printf("An asterisk (*) denotes that a network service is disabled.\n");
	for (i = 0; i < CFArrayGetCount(all); i++) {
		SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, i);
		SCNetworkInterfaceRef ifp = SCNetworkServiceGetInterface(s);

		printf("(%s%ld) %s\n", SCNetworkServiceGetEnabled(s) ? "" : "*",
		    (long)i + 1,
		    cstr(SCNetworkServiceGetName(s), name, sizeof(name)));
		printf("(Hardware Port: %s, Device: %s)\n\n",
		    cstr(ifp ? SCNetworkInterfaceGetLocalizedDisplayName(ifp) :
		    NULL, port, sizeof(port)),
		    cstr(ifp ? SCNetworkInterfaceGetBSDName(ifp) : NULL, dev,
		    sizeof(dev)));
	}
	CFRelease(all);
	return (0);
}

static int
list_hardware_ports(void)
{
	CFArrayRef all = SCNetworkInterfaceCopyAll();
	char port[256], dev[64], mac[64];
	CFIndex i;

	for (i = 0; all != NULL && i < CFArrayGetCount(all); i++) {
		SCNetworkInterfaceRef ifp = CFArrayGetValueAtIndex(all, i);

		printf("\nHardware Port: %s\nDevice: %s\nEthernet Address: %s\n",
		    cstr(SCNetworkInterfaceGetLocalizedDisplayName(ifp), port,
		    sizeof(port)),
		    cstr(SCNetworkInterfaceGetBSDName(ifp), dev, sizeof(dev)),
		    cstr(SCNetworkInterfaceGetHardwareAddressString(ifp), mac,
		    sizeof(mac)));
	}
	printf("\nVLAN Configurations\n===================\n");
	if (all != NULL)
		CFRelease(all);
	return (0);
}

static int
get_service_enabled(const char *name)
{
	SCNetworkServiceRef s = find_service(name);

	if (s == NULL)
		return (invalid());
	printf("%s\n", SCNetworkServiceGetEnabled(s) ? "Enabled" : "Disabled");
	CFRelease(s);
	return (0);
}

static bool
on_off(const char *a, bool *v)
{
	if (a != NULL && strcasecmp(a, "on") == 0)
		*v = true;
	else if (a != NULL && strcasecmp(a, "off") == 0)
		*v = false;
	else
		return (false);
	return (true);
}

static int
set_service_enabled(const char *name, const char *state)
{
	SCNetworkServiceRef s = find_service(name);
	bool on;

	if (s == NULL || !on_off(state, &on))
		return (invalid());
	if (!SCNetworkServiceSetEnabled(s, on)) {
		CFRelease(s);
		return (invalid());
	}
	CFRelease(s);
	changed = true;
	return (0);
}

static int
rename_service(const char *old, const char *new)
{
	SCNetworkServiceRef s = find_service(old);
	CFStringRef n;
	bool ok;

	if (s == NULL || new == NULL)
		return (invalid());
	n = cfstr(new);
	ok = SCNetworkServiceSetName(s, n);
	CFRelease(n);
	CFRelease(s);
	if (!ok) {
		printf("** Error: %s.\n", SCErrorString(SCError()));
		return (1);
	}
	changed = true;
	return (0);
}

static int
remove_service(const char *name)
{
	SCNetworkServiceRef s = find_service(name);

	if (s == NULL)
		return (invalid());
	if (!SCNetworkServiceRemove(s)) {
		CFRelease(s);
		return (invalid());
	}
	CFRelease(s);
	changed = true;
	return (0);
}

static int
create_service(const char *name, const char *port)
{
	SCNetworkInterfaceRef ifp;
	SCNetworkServiceRef s;
	SCNetworkSetRef set;
	CFStringRef n;
	bool ok;

	if (name == NULL || port == NULL ||
	    (ifp = find_interface(port)) == NULL) {
		if (port != NULL)
			printf("%s is not a valid hardware port.\n", port);
		return (invalid());
	}
	set = current_set();
	n = cfstr(name);
	ok = (s = SCNetworkServiceCreate(prefs, ifp)) != NULL &&
	    SCNetworkServiceEstablishDefaultConfiguration(s) &&
	    SCNetworkServiceSetName(s, n) && SCNetworkSetAddService(set, s);
	CFRelease(n);
	CFRelease(set);
	CFRelease(ifp);
	if (!ok) {
		printf("** Error: %s.\n", SCErrorString(SCError()));
		if (s != NULL)
			SCNetworkServiceRemove(s);
		return (1);
	}
	CFRelease(s);
	changed = true;
	return (0);
}

/* The order of every service of the location, by name. */
static int
order_services(char **names, int n)
{
	CFArrayRef all = services();
	CFMutableArrayRef order = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	SCNetworkSetRef set;
	int i;
	CFIndex j;
	bool ok;

	if (n != CFArrayGetCount(all)) {
		printf("You must specify all of the network services.\n");
		CFRelease(all);
		CFRelease(order);
		return (invalid());
	}
	for (i = 0; i < n; i++) {
		for (j = 0; j < CFArrayGetCount(all); j++) {
			SCNetworkServiceRef s = CFArrayGetValueAtIndex(all, j);

			if (cfeq(SCNetworkServiceGetName(s), names[i])) {
				CFArrayAppendValue(order,
				    SCNetworkServiceGetServiceID(s));
				break;
			}
		}
		if (j == CFArrayGetCount(all)) {
			printf("%s is not a recognized network service.\n",
			    names[i]);
			CFRelease(all);
			CFRelease(order);
			return (invalid());
		}
	}
	set = current_set();
	ok = SCNetworkSetSetServiceOrder(set, order);
	CFRelease(set);
	CFRelease(all);
	CFRelease(order);
	if (!ok)
		return (invalid());
	changed = true;
	return (0);
}

static int
detect_new_hardware(void)
{
	SCNetworkSetRef set = current_set();

	/* Adds a service for each interface that has none. */
	if (SCNetworkSetEstablishDefaultConfiguration(set))
		changed = true;
	CFRelease(set);
	return (0);
}

/* ---- IPv4 and IPv6 ------------------------------------------------------- */

/* What the service has now, as configd's dynamic store says. */
static CFDictionaryRef
state(SCNetworkServiceRef s, CFStringRef entity)
{
	CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
	    kSCDynamicStoreDomainState, SCNetworkServiceGetServiceID(s), entity);
	CFPropertyListRef v = SCDynamicStoreCopyValue(NULL, key);

	CFRelease(key);
	if (v != NULL && CFGetTypeID(v) != CFDictionaryGetTypeID()) {
		CFRelease(v);
		v = NULL;
	}
	return (v);
}

/* The interface's first IPv4 address and mask, when the store has none. */
static void
ifaddr(const char *ifname, char *addr, char *mask, size_t len)
{
	struct ifaddrs *ifa, *p;

	if (getifaddrs(&ifa) != 0)
		return;
	for (p = ifa; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL || p->ifa_addr->sa_family != AF_INET ||
		    strcmp(p->ifa_name, ifname) != 0)
			continue;
		inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)
		    p->ifa_addr)->sin_addr, addr, (socklen_t)len);
		if (p->ifa_netmask != NULL)
			inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)
			    p->ifa_netmask)->sin_addr, mask, (socklen_t)len);
		break;
	}
	freeifaddrs(ifa);
}

static int
get_info(const char *name)
{
	SCNetworkServiceRef s = find_service(name);
	SCNetworkInterfaceRef ifp;
	CFDictionaryRef v4, v6, st4, st6;
	CFStringRef method;
	char addr[64] = "", mask[64] = "", router[64] = "", buf[256], dev[64];

	if (s == NULL)
		return (invalid());
	ifp = SCNetworkServiceGetInterface(s);
	cstr(ifp ? SCNetworkInterfaceGetBSDName(ifp) : NULL, dev, sizeof(dev));
	v4 = protocol_config(s, kSCNetworkProtocolTypeIPv4);
	st4 = state(s, kSCEntNetIPv4);
	first_string(dict_get(st4, kSCPropNetIPv4Addresses), addr, sizeof(addr));
	first_string(dict_get(st4, kSCPropNetIPv4SubnetMasks), mask,
	    sizeof(mask));
	first_string(dict_get(st4, kSCPropNetIPv4Router), router,
	    sizeof(router));
	if (addr[0] == '\0' && dev[0] != '\0')
		ifaddr(dev, addr, mask, sizeof(addr));
	method = dict_get(v4, kSCPropNetIPv4ConfigMethod);
	if (!protocol_enabled(s, kSCNetworkProtocolTypeIPv4) || method == NULL)
		printf("IPv4 is off\n");
	else if (CFEqual(method, kSCValNetIPv4ConfigMethodManual))
		printf("Manual Configuration\n");
	else if (CFEqual(method, kSCValNetIPv4ConfigMethodINFORM))
		printf("Manually Using DHCP Router Configuration\n");
	else if (CFEqual(method, kSCValNetIPv4ConfigMethodBOOTP))
		printf("BOOTP Configuration\n");
	else
		printf("DHCP Configuration\n");
	if (protocol_enabled(s, kSCNetworkProtocolTypeIPv4) && method != NULL) {
		printf("IP address: %s\n", addr[0] ? addr : "none");
		printf("Subnet mask: %s\n", mask[0] ? mask : "none");
		printf("Router: %s\n", router[0] ? router : "none");
		if (method != NULL &&
		    CFEqual(method, kSCValNetIPv4ConfigMethodDHCP))
			printf("Client ID: %s\n", cstr(dict_get(v4,
			    kSCPropNetIPv4DHCPClientID), buf, sizeof(buf)));
	}
	v6 = protocol_config(s, kSCNetworkProtocolTypeIPv6);
	st6 = state(s, kSCEntNetIPv6);
	if (!protocol_enabled(s, kSCNetworkProtocolTypeIPv6))
		printf("IPv6: Off\n");
	else
		printf("IPv6: %s\n", cstr(dict_get(v6,
		    kSCPropNetIPv6ConfigMethod), buf, sizeof(buf)));
	first_string(dict_get(st6, kSCPropNetIPv6Addresses), addr, sizeof(addr));
	printf("IPv6 IP address: %s\n", addr[0] ? addr : "none");
	first_string(dict_get(st6, kSCPropNetIPv6Router), router,
	    sizeof(router));
	printf("IPv6 Router: %s\n", router[0] ? router : "none");
	if (ifp != NULL && SCNetworkInterfaceGetHardwareAddressString(ifp))
		printf("Ethernet Address: %s\n",
		    cstr(SCNetworkInterfaceGetHardwareAddressString(ifp), buf,
		    sizeof(buf)));
	if (v4 != NULL)
		CFRelease(v4);
	if (v6 != NULL)
		CFRelease(v6);
	if (st4 != NULL)
		CFRelease(st4);
	if (st6 != NULL)
		CFRelease(st6);
	CFRelease(s);
	return (0);
}

static bool
valid_ipv4(const char *a)
{
	struct in_addr in;

	return (a != NULL && inet_pton(AF_INET, a, &in) == 1);
}

static int
set_ipv4(const char *name, CFStringRef method, const char *ip,
    const char *mask, const char *router, const char *clientid)
{
	SCNetworkServiceRef s = find_service(name);
	CFMutableDictionaryRef c;
	int rc;

	if (s == NULL)
		return (invalid());
	c = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(c, kSCPropNetIPv4ConfigMethod, method);
	if (ip != NULL) {
		CFArrayRef a = strings_array((char **)&ip, 1);

		CFDictionarySetValue(c, kSCPropNetIPv4Addresses, a);
		CFRelease(a);
	}
	if (mask != NULL) {
		CFArrayRef a = strings_array((char **)&mask, 1);

		CFDictionarySetValue(c, kSCPropNetIPv4SubnetMasks, a);
		CFRelease(a);
	}
	if (router != NULL) {
		CFStringRef r = cfstr(router);

		CFDictionarySetValue(c, kSCPropNetIPv4Router, r);
		CFRelease(r);
	}
	if (clientid != NULL) {
		CFStringRef id = cfstr(clientid);

		CFDictionarySetValue(c, kSCPropNetIPv4DHCPClientID, id);
		CFRelease(id);
	}
	rc = set_protocol(s, kSCNetworkProtocolTypeIPv4, c, true);
	CFRelease(c);
	CFRelease(s);
	return (rc);
}

static int
set_ipv6(const char *name, CFStringRef method)
{
	SCNetworkServiceRef s = find_service(name);
	CFMutableDictionaryRef c;
	int rc;

	if (s == NULL)
		return (invalid());
	if (method == NULL) {
		rc = set_protocol(s, kSCNetworkProtocolTypeIPv6, NULL, false);
	} else {
		c = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(c, kSCPropNetIPv6ConfigMethod, method);
		rc = set_protocol(s, kSCNetworkProtocolTypeIPv6, c, true);
		CFRelease(c);
	}
	CFRelease(s);
	return (rc);
}

/* ---- DNS --------------------------------------------------------------------- */

static int
get_dns(const char *name, CFStringRef key, const char *what)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef c;
	CFTypeRef v;

	if (s == NULL)
		return (invalid());
	c = protocol_config(s, kSCNetworkProtocolTypeDNS);
	v = dict_get(c, key);
	if (v == NULL || CFArrayGetCount(v) == 0)
		printf("There aren't any %s set on %s.\n", what, name);
	else
		print_strings(v);
	if (c != NULL)
		CFRelease(c);
	CFRelease(s);
	return (0);
}

/* "Empty" clears the list. */
static int
set_dns(const char *name, CFStringRef key, char **v, int n, bool addresses)
{
	SCNetworkServiceRef s = find_service(name);
	CFArrayRef a = NULL;
	int i, rc;

	if (s == NULL || n == 0)
		return (invalid());
	if (!(n == 1 && strcasecmp(v[0], "empty") == 0)) {
		for (i = 0; addresses && i < n; i++) {
			struct in6_addr in6;

			if (!valid_ipv4(v[i]) &&
			    inet_pton(AF_INET6, v[i], &in6) != 1) {
				printf("%s is not a valid IP address.\n", v[i]);
				CFRelease(s);
				return (invalid());
			}
		}
		a = strings_array(v, n);
	}
	rc = set_protocol_key(s, kSCNetworkProtocolTypeDNS, key, a);
	if (a != NULL)
		CFRelease(a);
	CFRelease(s);
	return (rc);
}

/* ---- Proxies ---------------------------------------------------------------- */

static const struct proxy {
	const char	*cmd;		/* -get<cmd>, -set<cmd>, -set<cmd>state */
	CFStringRef	 enable, server, port;
} proxies[] = {
	{ "webproxy", CFSTR("HTTPEnable"), CFSTR("HTTPProxy"),
	    CFSTR("HTTPPort") },
	{ "securewebproxy", CFSTR("HTTPSEnable"), CFSTR("HTTPSProxy"),
	    CFSTR("HTTPSPort") },
	{ "ftpproxy", CFSTR("FTPEnable"), CFSTR("FTPProxy"), CFSTR("FTPPort") },
	{ "socksfirewallproxy", CFSTR("SOCKSEnable"), CFSTR("SOCKSProxy"),
	    CFSTR("SOCKSPort") },
	{ "streamingproxy", CFSTR("RTSPEnable"), CFSTR("RTSPProxy"),
	    CFSTR("RTSPPort") },
	{ "gopherproxy", CFSTR("GopherEnable"), CFSTR("GopherProxy"),
	    CFSTR("GopherPort") },
};
#define NPROXIES	(sizeof(proxies) / sizeof(proxies[0]))

static int
get_proxy(const struct proxy *p, const char *name)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef c;
	char buf[256];

	if (s == NULL)
		return (invalid());
	c = protocol_config(s, kSCNetworkProtocolTypeProxies);
	printf("Enabled: %s\nServer: %s\nPort: %ld\n"
	    "Authenticated Proxy Enabled: 0\n",
	    dict_long(c, p->enable) ? "Yes" : "No",
	    cstr(dict_get(c, p->server), buf, sizeof(buf)),
	    dict_long(c, p->port));
	if (c != NULL)
		CFRelease(c);
	CFRelease(s);
	return (0);
}

static int
set_proxy(const struct proxy *p, const char *name, const char *server,
    const char *port)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef old;
	CFMutableDictionaryRef c;
	CFStringRef srv;
	CFNumberRef num;
	char *end;
	int one = 1, rc;
	long pn;

	if (s == NULL || server == NULL || port == NULL)
		return (invalid());
	pn = strtol(port, &end, 10);
	if (*end != '\0' || pn <= 0 || pn > 65535) {
		CFRelease(s);
		return (invalid());
	}
	old = protocol_config(s, kSCNetworkProtocolTypeProxies);
	c = old != NULL ? CFDictionaryCreateMutableCopy(NULL, 0, old) :
	    CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	srv = cfstr(server);
	CFDictionarySetValue(c, p->server, srv);
	CFRelease(srv);
	num = CFNumberCreate(NULL, kCFNumberLongType, &pn);
	CFDictionarySetValue(c, p->port, num);
	CFRelease(num);
	num = CFNumberCreate(NULL, kCFNumberIntType, &one);
	CFDictionarySetValue(c, p->enable, num);
	CFRelease(num);
	rc = set_protocol(s, kSCNetworkProtocolTypeProxies, c, true);
	CFRelease(c);
	if (old != NULL)
		CFRelease(old);
	CFRelease(s);
	return (rc);
}

static int
set_proxy_state(CFStringRef key, const char *name, const char *state)
{
	SCNetworkServiceRef s = find_service(name);
	CFNumberRef num;
	bool on;
	int v, rc;

	if (s == NULL || !on_off(state, &on))
		return (invalid());
	v = on;
	num = CFNumberCreate(NULL, kCFNumberIntType, &v);
	rc = set_protocol_key(s, kSCNetworkProtocolTypeProxies, key, num);
	CFRelease(num);
	CFRelease(s);
	return (rc);
}

static int
get_bypass(const char *name)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef c;
	CFTypeRef v;

	if (s == NULL)
		return (invalid());
	c = protocol_config(s, kSCNetworkProtocolTypeProxies);
	v = dict_get(c, kSCPropNetProxiesExceptionsList);
	if (v == NULL || CFArrayGetCount(v) == 0)
		printf("There aren't any bypass domains set on %s.\n", name);
	else
		print_strings(v);
	if (c != NULL)
		CFRelease(c);
	CFRelease(s);
	return (0);
}

static int
set_bypass(const char *name, char **v, int n)
{
	SCNetworkServiceRef s = find_service(name);
	CFArrayRef a = NULL;
	int rc;

	if (s == NULL || n == 0)
		return (invalid());
	if (!(n == 1 && strcasecmp(v[0], "empty") == 0))
		a = strings_array(v, n);
	rc = set_protocol_key(s, kSCNetworkProtocolTypeProxies,
	    kSCPropNetProxiesExceptionsList, a);
	if (a != NULL)
		CFRelease(a);
	CFRelease(s);
	return (rc);
}

static int
get_autoproxy(const char *name)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef c;
	char buf[512];

	if (s == NULL)
		return (invalid());
	c = protocol_config(s, kSCNetworkProtocolTypeProxies);
	cstr(dict_get(c, kSCPropNetProxiesProxyAutoConfigURLString), buf,
	    sizeof(buf));
	printf("URL: %s\nEnabled: %s\n", buf[0] ? buf : "(null)",
	    dict_long(c, kSCPropNetProxiesProxyAutoConfigEnable) ? "Yes" : "No");
	if (c != NULL)
		CFRelease(c);
	CFRelease(s);
	return (0);
}

static int
set_autoproxy(const char *name, const char *url)
{
	SCNetworkServiceRef s = find_service(name);
	CFStringRef u;
	CFNumberRef num;
	int one = 1, rc;

	if (s == NULL || url == NULL)
		return (invalid());
	u = cfstr(url);
	rc = set_protocol_key(s, kSCNetworkProtocolTypeProxies,
	    kSCPropNetProxiesProxyAutoConfigURLString, u);
	CFRelease(u);
	num = CFNumberCreate(NULL, kCFNumberIntType, &one);
	if (rc == 0)
		rc = set_protocol_key(s, kSCNetworkProtocolTypeProxies,
		    kSCPropNetProxiesProxyAutoConfigEnable, num);
	CFRelease(num);
	CFRelease(s);
	return (rc);
}

static int
get_autodiscovery(const char *name)
{
	SCNetworkServiceRef s = find_service(name);
	CFDictionaryRef c;

	if (s == NULL)
		return (invalid());
	c = protocol_config(s, kSCNetworkProtocolTypeProxies);
	printf("Auto Proxy Discovery: %s\n",
	    dict_long(c, kSCPropNetProxiesProxyAutoDiscoveryEnable) ? "On" :
	    "Off");
	if (c != NULL)
		CFRelease(c);
	CFRelease(s);
	return (0);
}

/* ---- Hardware ------------------------------------------------------------- */

static int
get_macaddress(const char *port)
{
	SCNetworkInterfaceRef ifp = port ? find_interface(port) : NULL;
	char mac[64], dev[64], name[256];

	if (ifp == NULL) {
		printf("%s is not a hardware port or device.\n",
		    port ? port : "");
		return (invalid());
	}
	printf("Ethernet Address: %s (Device: %s)\n",
	    cstr(SCNetworkInterfaceGetHardwareAddressString(ifp), mac,
	    sizeof(mac)),
	    cstr(SCNetworkInterfaceGetBSDName(ifp), dev, sizeof(dev)));
	(void)name;
	CFRelease(ifp);
	return (0);
}

/* The interface as the service of the current location has it, so that
 * a change is kept in the configuration. */
static SCNetworkInterfaceRef
configured_interface(const char *dev)
{
	CFArrayRef all = services();
	SCNetworkInterfaceRef found = NULL;
	CFIndex i;

	for (i = 0; dev != NULL && i < CFArrayGetCount(all); i++) {
		SCNetworkInterfaceRef ifp = SCNetworkServiceGetInterface(
		    CFArrayGetValueAtIndex(all, i));

		if (ifp != NULL && (cfeq(SCNetworkInterfaceGetBSDName(ifp), dev) ||
		    cfeq(SCNetworkInterfaceGetLocalizedDisplayName(ifp), dev))) {
			found = (SCNetworkInterfaceRef)CFRetain(ifp);
			break;
		}
	}
	CFRelease(all);
	if (found == NULL && dev != NULL)
		found = find_interface(dev);
	if (found == NULL)
		printf("%s is not a valid hardware port or device.\n",
		    dev ? dev : "");
	return (found);
}

static int
get_mtu(const char *dev, bool range)
{
	SCNetworkInterfaceRef ifp = configured_interface(dev);
	int cur = 0, min = 0, max = 0;

	if (ifp == NULL)
		return (invalid());
	if (!SCNetworkInterfaceCopyMTU(ifp, &cur, &min, &max)) {
		CFRelease(ifp);
		printf("** Error: Unable to get the MTU of %s.\n", dev);
		return (1);
	}
	if (range)
		printf("Valid MTU Range: %d-%d\n", min, max);
	else
		printf("Active MTU: %d (Current Setting: %d)\n", cur, cur);
	CFRelease(ifp);
	return (0);
}

static int
set_mtu(const char *dev, const char *value)
{
	SCNetworkInterfaceRef ifp = configured_interface(dev);
	int cur, min, max, mtu;
	char *end;

	if (ifp == NULL || value == NULL)
		return (invalid());
	mtu = (int)strtol(value, &end, 10);
	if (*end != '\0' || !SCNetworkInterfaceCopyMTU(ifp, &cur, &min, &max) ||
	    mtu < min || mtu > max) {
		printf("%s is not a valid MTU for %s.\n", value, dev);
		CFRelease(ifp);
		return (invalid());
	}
	if (!SCNetworkInterfaceSetMTU(ifp, mtu)) {
		CFRelease(ifp);
		return (invalid());
	}
	CFRelease(ifp);
	changed = true;
	return (0);
}

static void
print_media(CFDictionaryRef m)
{
	char sub[64], opt[64];
	CFTypeRef opts = dict_get(m, kSCPropNetEthernetMediaOptions);
	CFIndex i;

	printf("%s", cstr(dict_get(m, kSCPropNetEthernetMediaSubType), sub,
	    sizeof(sub)));
	if (opts != NULL && CFArrayGetCount(opts) > 0) {
		printf(" <");
		for (i = 0; i < CFArrayGetCount(opts); i++)
			printf("%s%s", i ? "," : "",
			    cstr(CFArrayGetValueAtIndex(opts, i), opt,
			    sizeof(opt)));
		printf(">");
	}
	printf("\n");
}

static int
get_media(const char *dev, bool list)
{
	SCNetworkInterfaceRef ifp = configured_interface(dev);
	CFDictionaryRef current = NULL, active = NULL;
	CFArrayRef available = NULL;
	CFIndex i;

	if (ifp == NULL)
		return (invalid());
	if (!SCNetworkInterfaceCopyMediaOptions(ifp, &current, &active,
	    &available, false)) {
		printf("** Error: Unable to get the media of %s.\n", dev);
		CFRelease(ifp);
		return (1);
	}
	if (list) {
		for (i = 0; available != NULL && i < CFArrayGetCount(available);
		    i++)
			print_media(CFArrayGetValueAtIndex(available, i));
	} else {
		printf("Current: ");
		print_media(current);
		printf("Active: ");
		print_media(active);
	}
	if (current != NULL)
		CFRelease(current);
	if (active != NULL)
		CFRelease(active);
	if (available != NULL)
		CFRelease(available);
	CFRelease(ifp);
	return (0);
}

static int
set_media(const char *dev, char **v, int n)
{
	SCNetworkInterfaceRef ifp = configured_interface(dev);
	CFStringRef sub;
	CFArrayRef opts;
	bool ok;

	if (ifp == NULL || n < 1)
		return (invalid());
	sub = cfstr(v[0]);
	opts = strings_array(v + 1, n - 1);
	ok = SCNetworkInterfaceSetMediaOptions(ifp, sub, opts);
	CFRelease(sub);
	CFRelease(opts);
	CFRelease(ifp);
	if (!ok)
		return (invalid());
	changed = true;
	return (0);
}

/* ---- Locations ----------------------------------------------------------- */

static SCNetworkSetRef
find_set(const char *name)
{
	CFArrayRef all = SCNetworkSetCopyAll(prefs);
	SCNetworkSetRef found = NULL;
	CFIndex i;

	for (i = 0; all != NULL && i < CFArrayGetCount(all); i++) {
		SCNetworkSetRef set = CFArrayGetValueAtIndex(all, i);

		if (cfeq(SCNetworkSetGetName(set), name)) {
			found = (SCNetworkSetRef)CFRetain(set);
			break;
		}
	}
	if (all != NULL)
		CFRelease(all);
	return (found);
}

static int
list_locations(void)
{
	CFArrayRef all = SCNetworkSetCopyAll(prefs);
	char buf[256];
	CFIndex i;

	for (i = 0; all != NULL && i < CFArrayGetCount(all); i++)
		printf("%s\n", cstr(SCNetworkSetGetName(
		    CFArrayGetValueAtIndex(all, i)), buf, sizeof(buf)));
	if (all != NULL)
		CFRelease(all);
	return (0);
}

static int
get_current_location(void)
{
	SCNetworkSetRef set = current_set();
	char buf[256];

	printf("%s\n", cstr(SCNetworkSetGetName(set), buf, sizeof(buf)));
	CFRelease(set);
	return (0);
}

static int
create_location(const char *name, const char *populate)
{
	SCNetworkSetRef set;
	CFStringRef n;
	bool ok;

	if (name == NULL || (populate != NULL &&
	    strcmp(populate, "populate") != 0))
		return (invalid());
	if ((set = find_set(name)) != NULL) {
		printf("%s is already a location.\n", name);
		CFRelease(set);
		return (invalid());
	}
	n = cfstr(name);
	ok = (set = SCNetworkSetCreate(prefs)) != NULL &&
	    SCNetworkSetSetName(set, n) &&
	    (populate == NULL || SCNetworkSetEstablishDefaultConfiguration(set));
	CFRelease(n);
	if (set != NULL)
		CFRelease(set);
	if (!ok)
		return (invalid());
	changed = true;
	return (0);
}

static int
delete_location(const char *name)
{
	SCNetworkSetRef set = find_set(name), cur = current_set();
	bool ok;

	if (set == NULL) {
		printf("%s is not a location.\n", name ? name : "");
		CFRelease(cur);
		return (invalid());
	}
	if (CFEqual(set, cur)) {
		printf("The current location can't be deleted.\n");
		CFRelease(set);
		CFRelease(cur);
		return (invalid());
	}
	ok = SCNetworkSetRemove(set);
	CFRelease(set);
	CFRelease(cur);
	if (!ok)
		return (invalid());
	changed = true;
	return (0);
}

static int
switch_location(const char *name)
{
	SCNetworkSetRef set = find_set(name);
	bool ok;

	if (set == NULL) {
		printf("%s is not a location.\n", name ? name : "");
		return (invalid());
	}
	ok = SCNetworkSetSetCurrent(set);
	CFRelease(set);
	if (!ok)
		return (invalid());
	printf("found it!\n");
	changed = true;
	return (0);
}

/* ---- Computer name ------------------------------------------------------- */

static int
get_computername(void)
{
	CFStringRef n = SCDynamicStoreCopyComputerName(NULL, NULL);
	char buf[256];

	printf("%s\n", cstr(n, buf, sizeof(buf)));
	if (n != NULL)
		CFRelease(n);
	return (0);
}

static int
set_computername(const char *name)
{
	CFStringRef n;
	bool ok;

	if (name == NULL || name[0] == '\0')
		return (invalid());
	n = cfstr(name);
	ok = SCPreferencesSetComputerName(prefs, n, kCFStringEncodingUTF8);
	CFRelease(n);
	if (!ok)
		return (invalid());
	changed = true;
	return (0);
}

/* ---- Wi-Fi ---------------------------------------------------------------- */

static int
not_wifi(const char *dev)
{
	printf("%s is not a Wi-Fi interface.\n", dev ? dev : "");
	printf("** Error: Error obtaining wireless information.\n");
	return (1);
}

/* ---- Commands ------------------------------------------------------------- */

static const struct command {
	const char	*name;
	const char	*args;
	int		 min, max;	/* argument count; max -1: any */
	bool		 writes;
} commands[] = {
	{ "-listnetworkserviceorder", "", 0, 0, false },
	{ "-listallnetworkservices", "", 0, 0, false },
	{ "-listallhardwareports", "", 0, 0, false },
	{ "-detectnewhardware", "", 0, 0, true },
	{ "-getmacaddress", "<hardwareport or device name>", 1, 1, false },
	{ "-getcomputername", "", 0, 0, false },
	{ "-setcomputername", "<name>", 1, 1, true },
	{ "-getinfo", "<networkservice>", 1, 1, false },
	{ "-setmanual", "<networkservice> <ip> <subnet> <router address>", 4, 4,
	    true },
	{ "-setdhcp", "<networkservice> [clientid]", 1, 2, true },
	{ "-setbootp", "<networkservice>", 1, 1, true },
	{ "-setmanualwithdhcprouter", "<networkservice> <ip>", 2, 2, true },
	{ "-setv4off", "<networkservice>", 1, 1, true },
	{ "-setv6off", "<networkservice>", 1, 1, true },
	{ "-setv6automatic", "<networkservice>", 1, 1, true },
	{ "-setv6LinkLocal", "<networkservice>", 1, 1, true },
	{ "-getdnsservers", "<networkservice>", 1, 1, false },
	{ "-setdnsservers", "<networkservice> <dns1> [dns2] [...]", 2, -1, true },
	{ "-getsearchdomains", "<networkservice>", 1, 1, false },
	{ "-setsearchdomains", "<networkservice> <domain1> [domain2] [...]", 2,
	    -1, true },
	{ "-getwebproxy", "<networkservice>", 1, 1, false },
	{ "-setwebproxy", "<networkservice> <domain> <port number>", 3, 3, true },
	{ "-setwebproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getsecurewebproxy", "<networkservice>", 1, 1, false },
	{ "-setsecurewebproxy", "<networkservice> <domain> <port number>", 3, 3,
	    true },
	{ "-setsecurewebproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getftpproxy", "<networkservice>", 1, 1, false },
	{ "-setftpproxy", "<networkservice> <domain> <port number>", 3, 3, true },
	{ "-setftpproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getsocksfirewallproxy", "<networkservice>", 1, 1, false },
	{ "-setsocksfirewallproxy", "<networkservice> <domain> <port number>",
	    3, 3, true },
	{ "-setsocksfirewallproxystate", "<networkservice> <on off>", 2, 2,
	    true },
	{ "-getstreamingproxy", "<networkservice>", 1, 1, false },
	{ "-setstreamingproxy", "<networkservice> <domain> <port number>", 3, 3,
	    true },
	{ "-setstreamingproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getgopherproxy", "<networkservice>", 1, 1, false },
	{ "-setgopherproxy", "<networkservice> <domain> <port number>", 3, 3,
	    true },
	{ "-setgopherproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getproxybypassdomains", "<networkservice>", 1, 1, false },
	{ "-setproxybypassdomains", "<networkservice> <domain1> [domain2] [...]",
	    2, -1, true },
	{ "-getautoproxyurl", "<networkservice>", 1, 1, false },
	{ "-setautoproxyurl", "<networkservice> <url>", 2, 2, true },
	{ "-setautoproxystate", "<networkservice> <on off>", 2, 2, true },
	{ "-getproxyautodiscovery", "<networkservice>", 1, 1, false },
	{ "-setproxyautodiscovery", "<networkservice> <on off>", 2, 2, true },
	{ "-getnetworkserviceenabled", "<networkservice>", 1, 1, false },
	{ "-setnetworkserviceenabled", "<networkservice> <on off>", 2, 2, true },
	{ "-createnetworkservice", "<networkservice> <hardwareport>", 2, 2,
	    true },
	{ "-renamenetworkservice", "<networkservice> <newnetworkservicename>",
	    2, 2, true },
	{ "-removenetworkservice", "<networkservice>", 1, 1, true },
	{ "-ordernetworkservices", "<service1> <service2> <service3> <...>", 1,
	    -1, true },
	{ "-getMTU", "<hardwareport or device name>", 1, 1, false },
	{ "-setMTU", "<hardwareport or device name> <value>", 2, 2, true },
	{ "-listvalidMTUrange", "<hardwareport or device name>", 1, 1, false },
	{ "-getmedia", "<hardwareport or device name>", 1, 1, false },
	{ "-setmedia", "<hardwareport or device name> <subtype> [option1] "
	    "[option2] [...]", 2, -1, true },
	{ "-listvalidmedia", "<hardwareport or device name>", 1, 1, false },
	{ "-listlocations", "", 0, 0, false },
	{ "-getcurrentlocation", "", 0, 0, false },
	{ "-createlocation", "<location name> [populate]", 1, 2, true },
	{ "-deletelocation", "<location name>", 1, 1, true },
	{ "-switchtolocation", "<location name>", 1, 1, true },
	{ "-getairportnetwork", "<device name>", 1, 1, false },
	{ "-setairportnetwork", "<device name> <network> [password]", 2, 3,
	    true },
	{ "-getairportpower", "<device name>", 1, 1, false },
	{ "-setairportpower", "<device name> <on off>", 2, 2, true },
	{ "-listpreferredwirelessnetworks", "<device name>", 1, 1, false },
};
#define NCOMMANDS	(sizeof(commands) / sizeof(commands[0]))

static void
print_commands(void)
{
	size_t i;

	for (i = 0; i < NCOMMANDS; i++)
		printf("networksetup %s%s%s\n", commands[i].name,
		    commands[i].args[0] ? " " : "", commands[i].args);
}

static int
run(const char *cmd, char **a, int n)
{
	size_t i;

	if (strcmp(cmd, "-listnetworkserviceorder") == 0)
		return (list_service_order());
	if (strcmp(cmd, "-listallnetworkservices") == 0)
		return (list_services());
	if (strcmp(cmd, "-listallhardwareports") == 0)
		return (list_hardware_ports());
	if (strcmp(cmd, "-detectnewhardware") == 0)
		return (detect_new_hardware());
	if (strcmp(cmd, "-getmacaddress") == 0)
		return (get_macaddress(a[0]));
	if (strcmp(cmd, "-getcomputername") == 0)
		return (get_computername());
	if (strcmp(cmd, "-setcomputername") == 0)
		return (set_computername(a[0]));
	if (strcmp(cmd, "-getinfo") == 0)
		return (get_info(a[0]));
	if (strcmp(cmd, "-setmanual") == 0) {
		if (!valid_ipv4(a[1]) || !valid_ipv4(a[2]) || !valid_ipv4(a[3]))
			return (invalid());
		return (set_ipv4(a[0], kSCValNetIPv4ConfigMethodManual, a[1],
		    a[2], a[3], NULL));
	}
	if (strcmp(cmd, "-setdhcp") == 0)
		return (set_ipv4(a[0], kSCValNetIPv4ConfigMethodDHCP, NULL, NULL,
		    NULL, n > 1 ? a[1] : NULL));
	if (strcmp(cmd, "-setbootp") == 0)
		return (set_ipv4(a[0], kSCValNetIPv4ConfigMethodBOOTP, NULL,
		    NULL, NULL, NULL));
	if (strcmp(cmd, "-setmanualwithdhcprouter") == 0) {
		if (!valid_ipv4(a[1]))
			return (invalid());
		return (set_ipv4(a[0], kSCValNetIPv4ConfigMethodINFORM, a[1],
		    NULL, NULL, NULL));
	}
	if (strcmp(cmd, "-setv4off") == 0) {
		SCNetworkServiceRef s = find_service(a[0]);
		int rc;

		if (s == NULL)
			return (invalid());
		rc = set_protocol(s, kSCNetworkProtocolTypeIPv4, NULL, false);
		CFRelease(s);
		return (rc);
	}
	if (strcmp(cmd, "-setv6off") == 0)
		return (set_ipv6(a[0], NULL));
	if (strcmp(cmd, "-setv6automatic") == 0)
		return (set_ipv6(a[0], kSCValNetIPv6ConfigMethodAutomatic));
	if (strcmp(cmd, "-setv6LinkLocal") == 0)
		return (set_ipv6(a[0], kSCValNetIPv6ConfigMethodLinkLocal));
	if (strcmp(cmd, "-getdnsservers") == 0)
		return (get_dns(a[0], kSCPropNetDNSServerAddresses,
		    "DNS Servers"));
	if (strcmp(cmd, "-setdnsservers") == 0)
		return (set_dns(a[0], kSCPropNetDNSServerAddresses, a + 1, n - 1,
		    true));
	if (strcmp(cmd, "-getsearchdomains") == 0)
		return (get_dns(a[0], kSCPropNetDNSSearchDomains,
		    "Search Domains"));
	if (strcmp(cmd, "-setsearchdomains") == 0)
		return (set_dns(a[0], kSCPropNetDNSSearchDomains, a + 1, n - 1,
		    false));
	for (i = 0; i < NPROXIES; i++) {
		const struct proxy *p = &proxies[i];
		size_t len = strlen(p->cmd);

		if (strncmp(cmd + 4, p->cmd, len) != 0)
			continue;
		if (strncmp(cmd, "-get", 4) == 0 && cmd[4 + len] == '\0')
			return (get_proxy(p, a[0]));
		if (strncmp(cmd, "-set", 4) == 0 && cmd[4 + len] == '\0')
			return (set_proxy(p, a[0], a[1], a[2]));
		if (strncmp(cmd, "-set", 4) == 0 &&
		    strcmp(cmd + 4 + len, "state") == 0)
			return (set_proxy_state(p->enable, a[0], a[1]));
	}
	if (strcmp(cmd, "-getproxybypassdomains") == 0)
		return (get_bypass(a[0]));
	if (strcmp(cmd, "-setproxybypassdomains") == 0)
		return (set_bypass(a[0], a + 1, n - 1));
	if (strcmp(cmd, "-getautoproxyurl") == 0)
		return (get_autoproxy(a[0]));
	if (strcmp(cmd, "-setautoproxyurl") == 0)
		return (set_autoproxy(a[0], a[1]));
	if (strcmp(cmd, "-setautoproxystate") == 0)
		return (set_proxy_state(kSCPropNetProxiesProxyAutoConfigEnable,
		    a[0], a[1]));
	if (strcmp(cmd, "-getproxyautodiscovery") == 0)
		return (get_autodiscovery(a[0]));
	if (strcmp(cmd, "-setproxyautodiscovery") == 0)
		return (set_proxy_state(kSCPropNetProxiesProxyAutoDiscoveryEnable,
		    a[0], a[1]));
	if (strcmp(cmd, "-getnetworkserviceenabled") == 0)
		return (get_service_enabled(a[0]));
	if (strcmp(cmd, "-setnetworkserviceenabled") == 0)
		return (set_service_enabled(a[0], a[1]));
	if (strcmp(cmd, "-createnetworkservice") == 0)
		return (create_service(a[0], a[1]));
	if (strcmp(cmd, "-renamenetworkservice") == 0)
		return (rename_service(a[0], a[1]));
	if (strcmp(cmd, "-removenetworkservice") == 0)
		return (remove_service(a[0]));
	if (strcmp(cmd, "-ordernetworkservices") == 0)
		return (order_services(a, n));
	if (strcmp(cmd, "-getMTU") == 0)
		return (get_mtu(a[0], false));
	if (strcmp(cmd, "-listvalidMTUrange") == 0)
		return (get_mtu(a[0], true));
	if (strcmp(cmd, "-setMTU") == 0)
		return (set_mtu(a[0], a[1]));
	if (strcmp(cmd, "-getmedia") == 0)
		return (get_media(a[0], false));
	if (strcmp(cmd, "-listvalidmedia") == 0)
		return (get_media(a[0], true));
	if (strcmp(cmd, "-setmedia") == 0)
		return (set_media(a[0], a + 1, n - 1));
	if (strcmp(cmd, "-listlocations") == 0)
		return (list_locations());
	if (strcmp(cmd, "-getcurrentlocation") == 0)
		return (get_current_location());
	if (strcmp(cmd, "-createlocation") == 0)
		return (create_location(a[0], n > 1 ? a[1] : NULL));
	if (strcmp(cmd, "-deletelocation") == 0)
		return (delete_location(a[0]));
	if (strcmp(cmd, "-switchtolocation") == 0)
		return (switch_location(a[0]));
	/* The AirPort commands: there is no Wi-Fi yet. */
	return (not_wifi(a[0]));
}

static void
usage(void)
{
	fprintf(stderr, "Usage: networksetup -<command> [arguments]\n"
	    "       networksetup -printcommands\n"
	    "       networksetup -help\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	size_t c;
	int n, rc;

	if (argc < 2)
		usage();
	if (strcasecmp(argv[1], "-printcommands") == 0 ||
	    strcasecmp(argv[1], "-help") == 0) {
		print_commands();
		return (0);
	}
	for (c = 0; c < NCOMMANDS; c++)
		if (strcasecmp(argv[1], commands[c].name) == 0)
			break;
	if (c == NCOMMANDS) {
		fprintf(stderr, "%s is not a valid command.\n", argv[1]);
		usage();
	}
	n = argc - 2;
	if (n < commands[c].min || (commands[c].max >= 0 &&
	    n > commands[c].max))
		return (invalid());
	if (commands[c].writes && geteuid() != 0) {
		printf("** Error: Command requires admin privileges.\n");
		return (1);
	}
	open_prefs();
	if (commands[c].writes && !SCPreferencesLock(prefs, TRUE)) {
		printf("** Error: Unable to lock the network database.\n");
		return (1);
	}
	rc = run(commands[c].name, argv + 2, n);
	if (rc == 0)
		rc = save();
	if (commands[c].writes)
		SCPreferencesUnlock(prefs);
	CFRelease(prefs);
	return (rc);
}
