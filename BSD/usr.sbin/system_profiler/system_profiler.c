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
 * system_profiler: the machine's hardware and software, by data type, as
 * text, a property list (-xml) or JSON (-json). The data types are those
 * this system can describe: the hardware overview (sysctl and the
 * platform's registry entry), the network interfaces (getifaddrs and
 * their media), the system software (SystemVersion.plist, the kernel, the
 * boot volume, SIP) and the mounted local volumes with their drives (the
 * IOMedia they come from). -detailLevel mini leaves out what identifies
 * the machine or its user.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

#include <sys/types.h>
#include <sys/attr.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <ifaddrs.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef uint32_t csr_config_t;
int csr_get_active_config(csr_config_t *config);	/* libsystem_kernel */

/* ---- What a data type reports: items of ordered fields --------------- */

#define MAXFIELDS	24

struct item;

struct field {
	const char	*key;		/* the property list's key */
	const char	*label;		/* the text's label */
	char		*text;		/* the value as text */
	CFTypeRef	 value;		/* the value in the property list */
	struct item	*sub;		/* or a group of fields */
	int		 personal;	/* left out at -detailLevel mini */
};

struct item {
	char		*name;		/* the text's heading */
	const char	*key;		/* its _name; the heading when NULL */
	int		 nfields;
	struct field	 fields[MAXFIELDS];
};

struct datatype {
	const char	*name;		/* SP...DataType */
	const char	*title;		/* the text's heading */
	int		 nitems;
	struct item	**items;
};

static int mini;

static struct item *
item_new(const char *name, const char *key)
{
	struct item *it = calloc(1, sizeof(*it));

	if (it == NULL)
		err(1, NULL);
	it->name = strdup(name);
	it->key = key;
	return (it);
}

static struct field *
field_add(struct item *it, const char *key, const char *label, int personal)
{
	struct field *f;

	if (it->nfields == MAXFIELDS)
		errx(1, "too many fields for %s", it->name);
	f = &it->fields[it->nfields++];
	f->key = key;
	f->label = label;
	f->personal = personal;
	return (f);
}

static CFStringRef
cfstr(const char *s)
{
	return (CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8));
}

/* A field shown as text and kept in the property list as the same string. */
static void
add_str(struct item *it, const char *key, const char *label, int personal,
    const char *fmt, ...)
{
	struct field *f = field_add(it, key, label, personal);
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(&f->text, fmt, ap) < 0)
		err(1, NULL);
	va_end(ap);
	f->value = cfstr(f->text);
}

/* A field whose text and property list value differ. */
static void
add_val(struct item *it, const char *key, const char *label, int personal,
    const char *text, CFTypeRef value)
{
	struct field *f = field_add(it, key, label, personal);

	f->text = strdup(text);
	f->value = value;
}

static void
add_sub(struct item *it, const char *key, const char *label, struct item *sub)
{
	if (sub->nfields > 0)
		field_add(it, key, label, 0)->sub = sub;
}

static void
datatype_add(struct datatype *dt, struct item *it)
{
	dt->items = realloc(dt->items, sizeof(*dt->items) * (dt->nitems + 1));
	if (dt->items == NULL)
		err(1, NULL);
	dt->items[dt->nitems++] = it;
}

/* ---- Small helpers ------------------------------------------------------ */

static int
sysctl_str(const char *name, char *buf, size_t len)
{
	if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
		return (-1);
	buf[len - 1] = '\0';
	return (0);
}

static int64_t
sysctl_int(const char *name)
{
	int64_t v = 0;
	size_t len = sizeof(v);

	if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
		return (-1);
	if (len == sizeof(int32_t))
		return (*(int32_t *)(void *)&v);
	return (v);
}

/* "2.30" -> "2.3", "3.00" -> "3": as macOS shows sizes and speeds. */
static void
trim_number(char *s)
{
	char *dot = strchr(s, '.'), *e;

	if (dot == NULL)
		return;
	for (e = s + strlen(s) - 1; e > dot && *e == '0'; e--)
		*e = '\0';
	if (e == dot)
		*e = '\0';
}

/* Decimal units, as the Finder and system_profiler count disk space. */
static void
format_size(uint64_t bytes, char *buf, size_t len)
{
	static const char *units[] = { "bytes", "KB", "MB", "GB", "TB", "PB" };
	double v = (double)bytes;
	unsigned u = 0;
	char n[32];

	while (v >= 1000.0 && u < 5) {
		v /= 1000.0;
		u++;
	}
	if (u == 0) {
		snprintf(buf, len, "%llu bytes", (unsigned long long)bytes);
		return;
	}
	snprintf(n, sizeof(n), "%.2f", v);
	trim_number(n);
	snprintf(buf, len, "%s %s", n, units[u]);
}

/* 1234567 -> "1,234,567" */
static void
format_grouped(uint64_t v, char *buf, size_t len)
{
	char digits[32], *p = buf;
	int n, i;

	n = snprintf(digits, sizeof(digits), "%llu", (unsigned long long)v);
	for (i = 0; i < n && (size_t)(p - buf) < len - 2; i++) {
		if (i > 0 && (n - i) % 3 == 0)
			*p++ = ',';
		*p++ = digits[i];
	}
	*p = '\0';
}

/* Binary units, as memory and caches are counted; memory a little short
 * of a whole gigabyte (what the firmware keeps) still counts as it. */
static void
format_binary(uint64_t bytes, char *buf, size_t len)
{
	if (bytes >= (1ULL << 30))
		snprintf(buf, len, "%llu GB",
		    (unsigned long long)((bytes + (1ULL << 29)) >> 30));
	else if (bytes >= (1ULL << 20))
		snprintf(buf, len, "%llu MB", (unsigned long long)(bytes >> 20));
	else
		snprintf(buf, len, "%llu KB", (unsigned long long)(bytes >> 10));
}

static CFTypeRef
registry_property(io_registry_entry_t e, const char *name)
{
	CFStringRef key = cfstr(name);
	CFTypeRef v = IORegistryEntryCreateCFProperty(e, key, NULL, 0);

	CFRelease(key);
	return (v);
}

/* A registry property (string or data) as a C string. */
static int
registry_string(io_registry_entry_t e, const char *name, char *buf, size_t len)
{
	CFTypeRef v = registry_property(e, name);
	int ok = 0;

	if (v == NULL)
		return (-1);
	if (CFGetTypeID(v) == CFStringGetTypeID()) {
		ok = CFStringGetCString(v, buf, (CFIndex)len,
		    kCFStringEncodingUTF8) ? 0 : -1;
	} else if (CFGetTypeID(v) == CFDataGetTypeID()) {
		size_t n = (size_t)CFDataGetLength(v);

		if (n >= len)
			n = len - 1;
		memcpy(buf, CFDataGetBytePtr(v), n);
		buf[n] = '\0';
	} else {
		ok = -1;
	}
	CFRelease(v);
	return (ok == 0 && buf[0] != '\0' ? 0 : -1);
}

static int
dict_string(CFDictionaryRef d, const char *name, char *buf, size_t len)
{
	CFStringRef key = cfstr(name);
	CFTypeRef v = d ? CFDictionaryGetValue(d, key) : NULL;

	CFRelease(key);
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return (-1);
	if (!CFStringGetCString(v, buf, (CFIndex)len, kCFStringEncodingUTF8))
		return (-1);
	/* Drives pad their names with spaces. */
	for (size_t n = strlen(buf); n > 0 && buf[n - 1] == ' '; n--)
		buf[n - 1] = '\0';
	return (buf[0] != '\0' ? 0 : -1);
}

/* ---- SPHardwareDataType ------------------------------------------------- */

/*
 * "Intel(R) Core(TM) i7-7700 CPU @ 3.60GHz" -> "Intel Core i7": the
 * trademarks, the model number and the speed go, as macOS names it.
 */
static void
processor_family(const char *brand, char *buf, size_t len)
{
	char tmp[128], *tok, *last, *at;

	/* Only Intel's and AMD's brands follow that pattern. */
	if (strncmp(brand, "Intel", 5) != 0 && strncmp(brand, "AMD", 3) != 0) {
		strlcpy(buf, brand, len);
		return;
	}
	strlcpy(tmp, brand, sizeof(tmp));
	if ((at = strstr(tmp, " @")) != NULL)
		*at = '\0';
	buf[0] = '\0';
	for (tok = strtok_r(tmp, " ", &last); tok != NULL;
	    tok = strtok_r(NULL, " ", &last)) {
		char *mark, *dash;

		while ((mark = strstr(tok, "(R)")) != NULL ||
		    (mark = strstr(tok, "(TM)")) != NULL)
			memmove(mark, strchr(mark, ')') + 1,
			    strlen(strchr(mark, ')') + 1) + 1);
		if (strcmp(tok, "CPU") == 0 || strcmp(tok, "Processor") == 0 ||
		    *tok == '\0')
			continue;
		if ((dash = strchr(tok, '-')) != NULL && dash != tok) {
			*dash = '\0';
			if (buf[0] != '\0')
				strlcat(buf, " ", len);
			strlcat(buf, tok, len);
			break;		/* the model number ends the family */
		}
		if (buf[0] != '\0')
			strlcat(buf, " ", len);
		strlcat(buf, tok, len);
	}
	if (buf[0] == '\0')
		strlcpy(buf, brand, len);
}

static double
brand_speed(const char *brand)
{
	const char *at = strstr(brand, "@ ");
	double ghz;

	if (at != NULL && sscanf(at + 2, "%lfGHz", &ghz) == 1)
		return (ghz);
	return (0);
}

static void
hardware(struct datatype *dt)
{
	struct item *it = item_new("Hardware Overview", "hardware_overview");
	io_registry_entry_t pe = IORegistryEntryFromPath(kIOMasterPortDefault,
	    kIOServicePlane ":/");
	io_registry_entry_t rom;
	char model[128], name[128], brand[128], family[128], buf[128], n[32];
	int64_t cores = sysctl_int("hw.physicalcpu");
	int64_t threads = sysctl_int("hw.logicalcpu");
	int64_t packages = sysctl_int("hw.packages");
	int64_t freq = sysctl_int("hw.cpufrequency");
	int64_t v;
	double ghz;

	if (sysctl_str("hw.model", model, sizeof(model)) != 0)
		strlcpy(model, "Unknown", sizeof(model));
	/* The platform's name for itself, when its firmware gives one. */
	if (pe != MACH_PORT_NULL &&
	    (registry_string(pe, "product-name", name, sizeof(name)) == 0 ||
	    registry_string(pe, "model", name, sizeof(name)) == 0))
		add_str(it, "machine_name", "Model Name", 0, "%s", name);
	add_str(it, "machine_model", "Model Identifier", 0, "%s", model);

	if (sysctl_str("machdep.cpu.brand_string", brand, sizeof(brand)) == 0) {
		processor_family(brand, family, sizeof(family));
		if (cores == 2)
			add_str(it, "cpu_type", "Processor Name", 0,
			    "Dual-Core %s", family);
		else if (cores == 4)
			add_str(it, "cpu_type", "Processor Name", 0,
			    "Quad-Core %s", family);
		else if (cores > 1)
			add_str(it, "cpu_type", "Processor Name", 0,
			    "%lld-Core %s", (long long)cores, family);
		else
			add_str(it, "cpu_type", "Processor Name", 0, "%s",
			    family);
		ghz = freq > 0 ? (double)freq / 1e9 : brand_speed(brand);
		if (ghz > 0) {
			snprintf(n, sizeof(n), "%.2f", ghz);
			trim_number(n);
			add_str(it, "current_processor_speed",
			    "Processor Speed", 0, "%s GHz", n);
		}
	}
	if (packages > 0)
		add_val(it, "packages", "Number of Processors", 0,
		    (snprintf(buf, sizeof(buf), "%lld", (long long)packages), buf),
		    CFNumberCreate(NULL, kCFNumberSInt64Type, &packages));
	if (cores > 0)
		add_val(it, "number_processors", "Total Number of Cores", 0,
		    (snprintf(buf, sizeof(buf), "%lld", (long long)cores), buf),
		    CFNumberCreate(NULL, kCFNumberSInt64Type, &cores));
	if ((v = sysctl_int("hw.l2cachesize")) > 0) {
		format_binary((uint64_t)v, buf, sizeof(buf));
		add_str(it, "l2_cache_core", "L2 Cache (per Core)", 0, "%s", buf);
	}
	if ((v = sysctl_int("hw.l3cachesize")) > 0) {
		format_binary((uint64_t)v, buf, sizeof(buf));
		add_str(it, "l3_cache", "L3 Cache", 0, "%s", buf);
	}
	if (threads > cores && cores > 0)
		add_val(it, "platform_cpu_htt", "Hyper-Threading Technology", 0,
		    "Enabled", cfstr("htt_enabled"));
	if ((v = sysctl_int("hw.memsize")) > 0) {
		format_binary((uint64_t)v, buf, sizeof(buf));
		add_str(it, "physical_memory", "Memory", 0, "%s", buf);
	}
	rom = IORegistryEntryFromPath(kIOMasterPortDefault,
	    kIODeviceTreePlane ":/rom");
	if (rom != MACH_PORT_NULL) {
		if (registry_string(rom, "version", buf, sizeof(buf)) == 0)
			add_str(it, "boot_rom_version", "Boot ROM Version", 0,
			    "%s", buf);
		IOObjectRelease(rom);
	}
	if (pe != MACH_PORT_NULL) {
		if (registry_string(pe, "IOPlatformSerialNumber", buf,
		    sizeof(buf)) == 0)
			add_str(it, "serial_number", "Serial Number (system)", 1,
			    "%s", buf);
		if (registry_string(pe, "IOPlatformUUID", buf, sizeof(buf)) == 0)
			add_str(it, "platform_UUID", "Hardware UUID", 1, "%s",
			    buf);
		IOObjectRelease(pe);
	}
	datatype_add(dt, it);
}

/* ---- SPNetworkDataType -------------------------------------------------- */

static const struct ifmedia_description media_subtypes[] =
    IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

static const char *
media_subtype(const char *ifname)
{
	struct ifmediareq ifmr;
	int s = socket(AF_INET, SOCK_DGRAM, 0), i, sub;

	if (s < 0)
		return (NULL);
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(s, SIOCGIFMEDIA, &ifmr) != 0) {
		close(s);
		return (NULL);
	}
	close(s);
	if ((ifmr.ifm_status & IFM_AVALID) && !(ifmr.ifm_status & IFM_ACTIVE))
		return ("none");
	sub = IFM_SUBTYPE(ifmr.ifm_active);
	for (i = 0; media_subtypes[i].ifmt_string != NULL; i++)
		if (media_subtypes[i].ifmt_word == sub)
			return (media_subtypes[i].ifmt_string);
	return (NULL);
}

static void
network(struct datatype *dt)
{
	struct ifaddrs *ifa, *p, *q;
	int ethernets = 0;

	if (getifaddrs(&ifa) != 0)
		return;
	for (p = ifa; p != NULL; p = p->ifa_next) {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)(void *)p->ifa_addr;
		struct item *it, *v4, *eth;
		char addrs[256] = "", masks[256] = "", mac[32] = "", name[64];
		CFMutableArrayRef a4, m4;
		const unsigned char *ll;
		const char *sub;

		if (sdl == NULL || sdl->sdl_family != AF_LINK ||
		    sdl->sdl_type != IFT_ETHER)
			continue;
		if (ethernets++ == 0)
			strlcpy(name, "Ethernet", sizeof(name));
		else
			snprintf(name, sizeof(name), "Ethernet %d", ethernets);
		it = item_new(name, NULL);
		add_str(it, "type", "Type", 0, "Ethernet");
		add_str(it, "hardware", "Hardware", 0, "Ethernet");
		add_str(it, "interface", "BSD Device Name", 0, "%s", p->ifa_name);

		a4 = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		m4 = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (q = ifa; q != NULL; q = q->ifa_next) {
			char a[INET_ADDRSTRLEN], m[INET_ADDRSTRLEN];
			CFStringRef s;

			if (strcmp(q->ifa_name, p->ifa_name) != 0 ||
			    q->ifa_addr == NULL || q->ifa_addr->sa_family != AF_INET)
				continue;
			inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)
			    q->ifa_addr)->sin_addr, a, sizeof(a));
			if (q->ifa_netmask != NULL)
				inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)
				    q->ifa_netmask)->sin_addr, m, sizeof(m));
			else
				strlcpy(m, "", sizeof(m));
			if (addrs[0] != '\0') {
				strlcat(addrs, ", ", sizeof(addrs));
				strlcat(masks, ", ", sizeof(masks));
			}
			strlcat(addrs, a, sizeof(addrs));
			strlcat(masks, m, sizeof(masks));
			CFArrayAppendValue(a4, (s = cfstr(a)));
			CFRelease(s);
			CFArrayAppendValue(m4, (s = cfstr(m)));
			CFRelease(s);
		}
		if (addrs[0] != '\0') {
			add_val(it, "ip_address", "IPv4 Addresses", 0, addrs,
			    CFRetain(a4));
			v4 = item_new("IPv4", NULL);
			add_val(v4, "Addresses", "Addresses", 0, addrs, a4);
			add_str(v4, "InterfaceName", "Interface Name", 0, "%s",
			    p->ifa_name);
			add_val(v4, "SubnetMasks", "Subnet Masks", 0, masks, m4);
			add_sub(it, "IPv4", "IPv4", v4);
		} else {
			CFRelease(a4);
			CFRelease(m4);
		}

		eth = item_new("Ethernet", NULL);
		ll = (const unsigned char *)LLADDR(sdl);
		if (sdl->sdl_alen == 6) {
			snprintf(mac, sizeof(mac),
			    "%02x:%02x:%02x:%02x:%02x:%02x",
			    ll[0], ll[1], ll[2], ll[3], ll[4], ll[5]);
			add_str(eth, "MAC Address", "MAC Address", 1, "%s", mac);
		}
		if ((sub = media_subtype(p->ifa_name)) != NULL)
			add_str(eth, "MediaSubType", "Media Subtype", 0, "%s", sub);
		add_sub(it, "Ethernet", "Ethernet", eth);
		datatype_add(dt, it);
	}
	freeifaddrs(ifa);
}

/* ---- SPSoftwareDataType ------------------------------------------------- */

struct volname {
	uint32_t	length;
	attrreference_t	name;
	char		buf[MAXPATHLEN];
} __attribute__((packed));

static int
volume_name(const char *path, char *buf, size_t len)
{
	struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .volattr = ATTR_VOL_INFO | ATTR_VOL_NAME };
	struct volname vn;

	if (getattrlist(path, &al, &vn, sizeof(vn), 0) != 0 ||
	    vn.name.attr_length == 0)
		return (-1);
	strlcpy(buf, (char *)&vn.name + vn.name.attr_dataoffset, len);
	return (buf[0] != '\0' ? 0 : -1);
}

struct voluuid {
	uint32_t	length;
	uuid_t		uuid;
} __attribute__((packed));

static int
volume_uuid(const char *path, char *buf, size_t len)
{
	struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .volattr = ATTR_VOL_INFO | ATTR_VOL_UUID };
	struct voluuid vu;
	static const uuid_t zero;
	const unsigned char *u = vu.uuid;

	if (getattrlist(path, &al, &vu, sizeof(vu), 0) != 0 ||
	    memcmp(vu.uuid, zero, sizeof(zero)) == 0)
		return (-1);
	snprintf(buf, len, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-"
	    "%02X%02X%02X%02X%02X%02X", u[0], u[1], u[2], u[3], u[4], u[5],
	    u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
	return (0);
}

static CFDictionaryRef
read_plist(const char *path)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
	    (const UInt8 *)path, (CFIndex)strlen(path), false);
	CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
	CFPropertyListRef pl = NULL;

	if (rs != NULL && CFReadStreamOpen(rs)) {
		pl = CFPropertyListCreateWithStream(NULL, rs, 0,
		    kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(rs);
	}
	if (rs != NULL)
		CFRelease(rs);
	CFRelease(url);
	if (pl != NULL && CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
		CFRelease(pl);
		pl = NULL;
	}
	return (pl);
}

static void
software(struct datatype *dt)
{
	struct item *it = item_new("System Software Overview", "os_overview");
	CFDictionaryRef sv = read_plist(
	    "/System/Library/CoreServices/SystemVersion.plist");
	char product[64], version[64], build[64], buf[256], host[256];
	struct passwd *pw = getpwuid(getuid());
	struct timeval boot;
	size_t len = sizeof(boot);
	struct xsw_usage swap;
	csr_config_t csr = 0;
	long up, d, h, m;

	if (dict_string(sv, "ProductName", product, sizeof(product)) == 0 &&
	    dict_string(sv, "ProductVersion", version, sizeof(version)) == 0) {
		if (dict_string(sv, "ProductBuildVersion", build,
		    sizeof(build)) == 0)
			add_str(it, "os_version", "System Version", 0,
			    "%s %s (%s)", product, version, build);
		else
			add_str(it, "os_version", "System Version", 0, "%s %s",
			    product, version);
	}
	if (sv != NULL)
		CFRelease(sv);
	if (sysctl_str("kern.osrelease", buf, sizeof(buf)) == 0)
		add_str(it, "kernel_version", "Kernel Version", 0, "Darwin %s",
		    buf);
	if (volume_name("/", buf, sizeof(buf)) == 0)
		add_str(it, "boot_volume", "Boot Volume", 0, "%s", buf);
	if (sysctl_int("kern.safeboot") > 0)
		add_val(it, "boot_mode", "Boot Mode", 0, "Safe",
		    cfstr("safe_boot"));
	else
		add_val(it, "boot_mode", "Boot Mode", 0, "Normal",
		    cfstr("normal_boot"));
	if (gethostname(host, sizeof(host)) == 0) {
		char *dot = strchr(host, '.');

		if (dot != NULL)
			*dot = '\0';
		add_str(it, "local_host_name", "Computer Name", 1, "%s", host);
	}
	if (pw != NULL) {
		char full[256];

		strlcpy(full, pw->pw_gecos ? pw->pw_gecos : "", sizeof(full));
		full[strcspn(full, ",")] = '\0';
		if (full[0] != '\0')
			add_str(it, "user_name", "User Name", 1, "%s (%s)", full,
			    pw->pw_name);
		else
			add_str(it, "user_name", "User Name", 1, "%s",
			    pw->pw_name);
	}
	len = sizeof(swap);
	if (sysctlbyname("vm.swapusage", &swap, &len, NULL, 0) == 0) {
		if (swap.xsu_encrypted)
			add_val(it, "secure_vm", "Secure Virtual Memory", 0,
			    "Enabled", cfstr("secure_vm_enabled"));
		else
			add_val(it, "secure_vm", "Secure Virtual Memory", 0,
			    "Disabled", cfstr("secure_vm_disabled"));
	}
	if (csr_get_active_config(&csr) == 0) {
		/* As csrutil reads it: no "allow" bit, all of them, or some. */
		if ((csr & 0x77) == 0)
			add_val(it, "system_integrity",
			    "System Integrity Protection", 0, "Enabled",
			    cfstr("integrity_enabled"));
		else if ((csr & 0x77) == 0x67)
			add_val(it, "system_integrity",
			    "System Integrity Protection", 0, "Disabled",
			    cfstr("integrity_disabled"));
		else
			add_val(it, "system_integrity",
			    "System Integrity Protection", 0,
			    "Custom Configuration", cfstr("integrity_custom"));
	}
	len = sizeof(boot);
	if (sysctlbyname("kern.boottime", &boot, &len, NULL, 0) == 0) {
		up = (long)(time(NULL) - boot.tv_sec);
		d = up / 86400;
		h = up % 86400 / 3600;
		m = up % 3600 / 60;
		if (d > 0)
			snprintf(buf, sizeof(buf), "%ld day%s %ld:%02ld", d,
			    d == 1 ? "" : "s", h, m);
		else if (h > 0)
			snprintf(buf, sizeof(buf), "%ld:%02ld", h, m);
		else
			snprintf(buf, sizeof(buf), "%ld minute%s", m,
			    m == 1 ? "" : "s");
		snprintf(host, sizeof(host), "up %ld:%ld:%ld:%ld", d, h, m,
		    up % 60);
		add_val(it, "uptime", "Time since boot", 0, buf, cfstr(host));
	}
	datatype_add(dt, it);
}

/* ---- SPStorageDataType -------------------------------------------------- */

static const char *
file_system(const struct statfs *sf)
{
	if (strcmp(sf->f_fstypename, "hfs") == 0)
		return (sf->f_flags & MNT_JOURNALED ? "Journaled HFS+" : "HFS+");
	if (strcmp(sf->f_fstypename, "msdos") == 0)
		return ("MS-DOS FAT");
	if (strcmp(sf->f_fstypename, "apfs") == 0)
		return ("APFS");
	if (strcmp(sf->f_fstypename, "exfat") == 0)
		return ("ExFAT");
	if (strcmp(sf->f_fstypename, "cd9660") == 0)
		return ("ISO 9660");
	if (strcmp(sf->f_fstypename, "udf") == 0)
		return ("UDF");
	return (sf->f_fstypename);
}

static const char *
partition_map(const char *content)
{
	if (strcmp(content, "GUID_partition_scheme") == 0)
		return ("GPT (GUID Partition Table)");
	if (strcmp(content, "FDisk_partition_scheme") == 0)
		return ("MBR (Master Boot Record)");
	if (strcmp(content, "Apple_partition_scheme") == 0)
		return ("APM (Apple Partition Map)");
	return (NULL);
}

/* The drive a volume's media is on: the IOMedia's ancestors in the
 * service plane up to the block storage device that describes it. */
static struct item *
physical_drive(const char *bsdname)
{
	struct item *pd = item_new("Physical Drive", NULL);
	io_registry_entry_t e, parent, whole = MACH_PORT_NULL;
	char buf[256];

	e = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOBSDNameMatching(kIOMasterPortDefault, 0, bsdname));
	while (e != MACH_PORT_NULL) {
		CFTypeRef dc = registry_property(e,
		    kIOPropertyDeviceCharacteristicsKey);
		CFTypeRef w = registry_property(e, kIOMediaWholeKey);

		if (w != NULL) {
			if (w == kCFBooleanTrue && whole == MACH_PORT_NULL) {
				whole = e;
				IOObjectRetain(whole);
			}
			CFRelease(w);
		}
		if (dc != NULL) {
			CFTypeRef pc = registry_property(e,
			    kIOPropertyProtocolCharacteristicsKey);

			if (dict_string(dc, kIOPropertyProductNameKey, buf,
			    sizeof(buf)) == 0)
				add_str(pd, "device_name", "Device Name", 0, "%s",
				    buf);
			if (whole != MACH_PORT_NULL) {
				io_name_t n;

				if (IORegistryEntryGetName(whole, n) ==
				    KERN_SUCCESS)
					add_str(pd, "media_name", "Media Name", 0,
					    "%s", n);
			}
			if (dict_string(dc, kIOPropertyMediumTypeKey, buf,
			    sizeof(buf)) == 0)
				add_str(pd, "medium_type", "Medium Type", 0, "%s",
				    buf);
			if (dict_string(pc, kIOPropertyPhysicalInterconnectTypeKey,
			    buf, sizeof(buf)) == 0)
				add_str(pd, "protocol", "Protocol", 0, "%s", buf);
			if (dict_string(pc,
			    kIOPropertyPhysicalInterconnectLocationKey, buf,
			    sizeof(buf)) == 0) {
				int internal = strcmp(buf,
				    kIOPropertyInternalKey) == 0;

				add_val(pd, "is_internal_disk", "Internal", 0,
				    internal ? "Yes" : "No",
				    cfstr(internal ? "yes" : "no"));
			}
			if (pc != NULL)
				CFRelease(pc);
			CFRelease(dc);
			IOObjectRelease(e);
			break;
		}
		if (IORegistryEntryGetParentEntry(e, kIOServicePlane, &parent) !=
		    KERN_SUCCESS)
			parent = MACH_PORT_NULL;
		IOObjectRelease(e);
		e = parent;
	}
	if (whole != MACH_PORT_NULL) {
		const char *map;

		if (registry_string(whole, kIOMediaContentKey, buf,
		    sizeof(buf)) == 0 && (map = partition_map(buf)) != NULL)
			add_val(pd, "partition_map_type", "Partition Map Type", 0,
			    map, cfstr(buf));
		IOObjectRelease(whole);
	}
	return (pd);
}

static void
storage(struct datatype *dt)
{
	struct statfs *sfs;
	int n, i;

	n = getfsstat(NULL, 0, MNT_NOWAIT);
	if (n <= 0)
		return;
	sfs = calloc((size_t)n, sizeof(*sfs));
	if (sfs == NULL)
		err(1, NULL);
	n = getfsstat(sfs, (int)(sizeof(*sfs) * (size_t)n), MNT_NOWAIT);
	for (i = 0; i < n; i++) {
		struct statfs *sf = &sfs[i];
		struct item *it;
		char name[MAXPATHLEN], size[32], grouped[32], buf[64];
		const char *bsd;
		uint64_t free_b, total_b;
		int64_t num;

		if (!(sf->f_flags & MNT_LOCAL) ||
		    strncmp(sf->f_mntfromname, "/dev/disk", 9) != 0)
			continue;
		bsd = sf->f_mntfromname + 5;
		if (volume_name(sf->f_mntonname, name, sizeof(name)) != 0)
			strlcpy(name, bsd, sizeof(name));
		it = item_new(name, NULL);
		free_b = (uint64_t)sf->f_bavail * sf->f_bsize;
		total_b = (uint64_t)sf->f_blocks * sf->f_bsize;
		format_size(free_b, size, sizeof(size));
		format_grouped(free_b, grouped, sizeof(grouped));
		snprintf(buf, sizeof(buf), "%s (%s bytes)", size, grouped);
		num = (int64_t)free_b;
		add_val(it, "free_space_in_bytes", "Free", 0, buf,
		    CFNumberCreate(NULL, kCFNumberSInt64Type, &num));
		format_size(total_b, size, sizeof(size));
		format_grouped(total_b, grouped, sizeof(grouped));
		snprintf(buf, sizeof(buf), "%s (%s bytes)", size, grouped);
		num = (int64_t)total_b;
		add_val(it, "size_in_bytes", "Capacity", 0, buf,
		    CFNumberCreate(NULL, kCFNumberSInt64Type, &num));
		add_str(it, "mount_point", "Mount Point", 0, "%s",
		    sf->f_mntonname);
		add_str(it, "file_system", "File System", 0, "%s",
		    file_system(sf));
		add_val(it, "writable", "Writable", 0,
		    sf->f_flags & MNT_RDONLY ? "No" : "Yes",
		    cfstr(sf->f_flags & MNT_RDONLY ? "no" : "yes"));
		add_val(it, "ignore_ownership", "Ignore Ownership", 0,
		    sf->f_flags & MNT_IGNORE_OWNERSHIP ? "Yes" : "No",
		    cfstr(sf->f_flags & MNT_IGNORE_OWNERSHIP ? "yes" : "no"));
		add_str(it, "bsd_name", "BSD Name", 0, "%s", bsd);
		if (volume_uuid(sf->f_mntonname, buf, sizeof(buf)) == 0)
			add_str(it, "volume_uuid", "Volume UUID", 1, "%s", buf);
		add_sub(it, "physical_drive", "Physical Drive",
		    physical_drive(bsd));
		datatype_add(dt, it);
	}
	free(sfs);
}

/* ---- The data types ------------------------------------------------------ */

static const struct {
	const char	*name;
	const char	*title;
	void		(*collect)(struct datatype *);
} datatypes[] = {
	{ "SPHardwareDataType",	"Hardware",	hardware },
	{ "SPNetworkDataType",	"Network",	network },
	{ "SPSoftwareDataType",	"Software",	software },
	{ "SPStorageDataType",	"Storage",	storage },
};
#define NDATATYPES	(sizeof(datatypes) / sizeof(datatypes[0]))

/* ---- Output -------------------------------------------------------------- */

static int
shown(const struct field *f)
{
	return (!(mini && f->personal));
}

static void
print_fields(const struct item *it, int indent)
{
	int i;

	for (i = 0; i < it->nfields; i++) {
		const struct field *f = &it->fields[i];

		if (!shown(f))
			continue;
		if (f->sub != NULL) {
			printf("%*s%s:\n", indent, "", f->label);
			print_fields(f->sub, indent + 4);
		} else {
			printf("%*s%s: %s\n", indent, "", f->label, f->text);
		}
	}
}

static void
print_text(struct datatype *dts, int ndts)
{
	int d, i;

	for (d = 0; d < ndts; d++) {
		if (dts[d].nitems == 0)
			continue;
		printf("%s:\n\n", dts[d].title);
		for (i = 0; i < dts[d].nitems; i++) {
			printf("    %s:\n\n", dts[d].items[i]->name);
			print_fields(dts[d].items[i], 6);
			printf("\n");
		}
	}
}

static CFDictionaryRef
item_dict(const struct item *it, int top)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFStringRef k, v;
	int i;

	if (top) {
		k = cfstr("_name");
		v = cfstr(it->key ? it->key : it->name);
		CFDictionarySetValue(d, k, v);
		CFRelease(k);
		CFRelease(v);
	}
	for (i = 0; i < it->nfields; i++) {
		const struct field *f = &it->fields[i];

		if (!shown(f))
			continue;
		k = cfstr(f->key);
		if (f->sub != NULL) {
			CFDictionaryRef sub = item_dict(f->sub, 0);

			CFDictionarySetValue(d, k, sub);
			CFRelease(sub);
		} else {
			CFDictionarySetValue(d, k, f->value);
		}
		CFRelease(k);
	}
	return (d);
}

static void
print_xml(struct datatype *dts, int ndts, const char *detail)
{
	CFMutableArrayRef root = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFDataRef data;
	int d, i;

	for (d = 0; d < ndts; d++) {
		CFMutableDictionaryRef dd = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		CFMutableArrayRef items = CFArrayCreateMutable(NULL, 0,
		    &kCFTypeArrayCallBacks);
		CFStringRef k, v;

		for (i = 0; i < dts[d].nitems; i++) {
			CFDictionaryRef id = item_dict(dts[d].items[i], 1);

			CFArrayAppendValue(items, id);
			CFRelease(id);
		}
		CFDictionarySetValue(dd, CFSTR("_items"), items);
		CFRelease(items);
		CFDictionarySetValue(dd, CFSTR("_dataType"),
		    (k = cfstr(dts[d].name)));
		CFRelease(k);
		CFDictionarySetValue(dd, CFSTR("_detailLevel"),
		    (v = cfstr(detail)));
		CFRelease(v);
		CFDictionarySetValue(dd, CFSTR("_parentDataType"),
		    CFSTR("SPRootDataType"));
		CFDictionarySetValue(dd, CFSTR("_timeStamp"), now);
		CFArrayAppendValue(root, dd);
		CFRelease(dd);
	}
	data = CFPropertyListCreateData(NULL, root, kCFPropertyListXMLFormat_v1_0,
	    0, NULL);
	if (data != NULL) {
		fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data),
		    stdout);
		CFRelease(data);
	}
	CFRelease(now);
	CFRelease(root);
}

static void
json_string(CFStringRef s)
{
	char buf[1024];
	const unsigned char *p;

	if (!CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
		buf[0] = '\0';
	putchar('"');
	for (p = (const unsigned char *)buf; *p != '\0'; p++) {
		if (*p == '"' || *p == '\\')
			printf("\\%c", *p);
		else if (*p < 0x20)
			printf("\\u%04x", *p);
		else
			putchar(*p);
	}
	putchar('"');
}

static void
json_value(CFTypeRef v, int indent);

static void
json_item(const struct item *it, int top, int indent)
{
	int i, first = 1;

	printf("{\n");
	if (top) {
		CFStringRef n = cfstr(it->key ? it->key : it->name);

		printf("%*s\"_name\" : ", indent + 2, "");
		json_string(n);
		CFRelease(n);
		first = 0;
	}
	for (i = 0; i < it->nfields; i++) {
		const struct field *f = &it->fields[i];
		CFStringRef k;

		if (!shown(f))
			continue;
		printf("%s%*s", first ? "" : ",\n", indent + 2, "");
		first = 0;
		k = cfstr(f->key);
		json_string(k);
		CFRelease(k);
		printf(" : ");
		if (f->sub != NULL)
			json_item(f->sub, 0, indent + 2);
		else
			json_value(f->value, indent + 2);
	}
	printf("\n%*s}", indent, "");
}

static void
json_value(CFTypeRef v, int indent)
{
	if (CFGetTypeID(v) == CFStringGetTypeID()) {
		json_string(v);
	} else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
		long long n = 0;

		CFNumberGetValue(v, kCFNumberLongLongType, &n);
		printf("%lld", n);
	} else if (CFGetTypeID(v) == CFArrayGetTypeID()) {
		CFIndex i, n = CFArrayGetCount(v);

		printf("[\n");
		for (i = 0; i < n; i++) {
			printf("%*s", indent + 2, "");
			json_value(CFArrayGetValueAtIndex(v, i), indent + 2);
			printf("%s\n", i + 1 < n ? "," : "");
		}
		printf("%*s]", indent, "");
	} else {
		printf("null");
	}
}

static void
print_json(struct datatype *dts, int ndts)
{
	int d, i;

	printf("{\n");
	for (d = 0; d < ndts; d++) {
		printf("  \"%s\" : [\n", dts[d].name);
		for (i = 0; i < dts[d].nitems; i++) {
			printf("    ");
			json_item(dts[d].items[i], 1, 4);
			printf("%s\n", i + 1 < dts[d].nitems ? "," : "");
		}
		printf("  ]%s\n", d + 1 < ndts ? "," : "");
	}
	printf("}\n");
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: system_profiler [-listDataTypes]\n"
	    "       system_profiler [-xml | -json] [-detailLevel level] "
	    "[-timeout seconds] [dataType ...]\n"
	    "  -listDataTypes  lists the available data types.\n"
	    "  -xml            generates a report in XML format.\n"
	    "  -json           generates a report in JSON format.\n"
	    "  -detailLevel    specifies the level of detail for the report:\n"
	    "                    mini = report with no personal information\n"
	    "                    basic = basic hardware and network information\n"
	    "                    full = all available information\n"
	    "  -timeout        specifies the maximum time to spend gathering "
	    "information.\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct datatype dts[NDATATYPES];
	const char *detail = "full";
	int ndts = 0, xml = 0, json = 0, named = 0, i;
	unsigned want = 0, t;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "-listDataTypes") == 0) {
			printf("Available Datatypes:\n");
			for (t = 0; t < NDATATYPES; t++)
				printf("%s\n", datatypes[t].name);
			return (0);
		} else if (strcmp(a, "-xml") == 0) {
			xml = 1;
		} else if (strcmp(a, "-json") == 0) {
			json = 1;
		} else if (strcmp(a, "-detailLevel") == 0) {
			if (++i == argc)
				usage();
			detail = argv[i];
			if (strcmp(detail, "mini") != 0 &&
			    strcmp(detail, "basic") != 0 &&
			    strcmp(detail, "full") != 0)
				usage();
			mini = strcmp(detail, "mini") == 0;
		} else if (strcmp(a, "-timeout") == 0) {
			if (++i == argc)
				usage();	/* we never wait for anything */
		} else if (strcmp(a, "-nospawn") == 0) {
			continue;
		} else if (strcmp(a, "-usage") == 0 || strcmp(a, "-help") == 0 ||
		    a[0] == '-') {
			usage();
		} else {
			/* An unknown data type is quietly not reported. */
			named = 1;
			for (t = 0; t < NDATATYPES; t++)
				if (strcmp(a, datatypes[t].name) == 0)
					want |= 1U << t;
		}
	}
	if (xml && json)
		usage();
	if (!named)
		want = (1U << NDATATYPES) - 1;
	for (t = 0; t < NDATATYPES; t++) {
		if (!(want & (1U << t)))
			continue;
		memset(&dts[ndts], 0, sizeof(dts[ndts]));
		dts[ndts].name = datatypes[t].name;
		dts[ndts].title = datatypes[t].title;
		datatypes[t].collect(&dts[ndts]);
		ndts++;
	}
	if (xml)
		print_xml(dts, ndts, detail);
	else if (json)
		print_json(dts, ndts);
	else
		print_text(dts, ndts);
	return (0);
}
