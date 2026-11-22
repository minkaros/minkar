/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network Menu Extra (Atlas.md, MB1): its icon says whether a wired
 * interface is connected; its menu has a submenu for each interface (the
 * loopback last) with its status, its IPv4 and IPv6 addresses and how
 * each is configured (the network services' ConfigMethod), and its
 * hardware address; choosing a line copies its value. Network Preferences
 * opens the pane. The icon looks again when the system's network
 * configuration changes (SystemConfiguration's State:/Network/Global/IPv4),
 * not on a timer; the menu is made as it opens.
 */

#import "MenuExtra.h"
#include <SystemConfiguration/SystemConfiguration.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[NetworkExtra class]] localizedStringForKey:(key) value:@"" table:nil]

@interface NetworkExtra : NSObject <MenuExtra, NSMenuDelegate>
- (void)update;
@end

static void networkChanged(SCDynamicStoreRef store, CFArrayRef keys, void *info)
{
    (void)store; (void)keys;
    NetworkExtra *extra = (__bridge NetworkExtra *)info;
    dispatch_async(dispatch_get_main_queue(), ^{ [extra update]; });
}

/* The interfaces up, other than loopback, each with its first IPv4
   address (or none). */
static NSArray<NSArray<NSString *> *> *interfacesUp(void)
{
    struct ifaddrs *list = NULL, *a;
    NSMutableDictionary<NSString *, NSString *> *found = [NSMutableDictionary dictionary];
    NSMutableArray<NSString *> *order = [NSMutableArray array];

    if (getifaddrs(&list) != 0)
        return @[];
    for (a = list; a != NULL; a = a->ifa_next) {
        if (!(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK) || a->ifa_name == NULL)
            continue;
        NSString *name = @(a->ifa_name);
        if (![order containsObject:name]) {
            [order addObject:name];
            found[name] = @"";
        }
        if (a->ifa_addr != NULL && a->ifa_addr->sa_family == AF_INET && [found[name] length] == 0) {
            char text[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)a->ifa_addr)->sin_addr, text, sizeof(text));
            found[name] = @(text);
        }
    }
    freeifaddrs(list);
    NSMutableArray *rows = [NSMutableArray array];
    for (NSString *name in order)
        if ([name hasPrefix:@"en"] || [found[name] length] > 0)
            [rows addObject:@[ name, found[name] ]];
    return rows;
}

/* One interface's details: status, addresses with their prefixes, the
   hardware address. */
static NSArray<NSDictionary *> *interfaceDetails(void)
{
    struct ifaddrs *list = NULL, *a;
    NSMutableDictionary<NSString *, NSMutableDictionary *> *found = [NSMutableDictionary dictionary];
    NSMutableArray<NSString *> *order = [NSMutableArray array];

    if (getifaddrs(&list) != 0)
        return @[];
    for (a = list; a != NULL; a = a->ifa_next) {
        if (a->ifa_name == NULL)
            continue;
        NSString *name = @(a->ifa_name);
        NSMutableDictionary *d = found[name];
        if (d == nil) {
            d = [NSMutableDictionary dictionaryWithObject:name forKey:@"name"];
            found[name] = d;
            [order addObject:name];
        }
        d[@"up"] = @((a->ifa_flags & IFF_UP) != 0 && (a->ifa_flags & IFF_RUNNING) != 0);
        d[@"loopback"] = @((a->ifa_flags & IFF_LOOPBACK) != 0);
        if (a->ifa_addr == NULL)
            continue;
        if (a->ifa_addr->sa_family == AF_INET && d[@"ipv4"] == nil) {
            char text[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)a->ifa_addr)->sin_addr, text, sizeof(text));
            int bits = 0;
            if (a->ifa_netmask != NULL) {
                uint32_t mask = ntohl(((struct sockaddr_in *)(void *)a->ifa_netmask)->sin_addr.s_addr);
                while (mask & 0x80000000u) { bits++; mask <<= 1; }
            }
            d[@"ipv4"] = [NSString stringWithFormat:@"%s/%d", text, bits];
        } else if (a->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *six = (struct sockaddr_in6 *)(void *)a->ifa_addr;
            char text[INET6_ADDRSTRLEN];
            struct in6_addr address = six->sin6_addr;
            /* A link-local address keeps its scope in its second word here:
               shown as it is written. */
            if (IN6_IS_ADDR_LINKLOCAL(&address))
                address.s6_addr[2] = address.s6_addr[3] = 0;
            inet_ntop(AF_INET6, &address, text, sizeof(text));
            int bits = 0;
            if (a->ifa_netmask != NULL) {
                const uint8_t *m = ((struct sockaddr_in6 *)(void *)a->ifa_netmask)->sin6_addr.s6_addr;
                for (int i = 0; i < 16; i++)
                    for (int b = 7; b >= 0 && (m[i] >> b) & 1; b--)
                        bits++;
            }
            NSString *value = [NSString stringWithFormat:@"%s/%d", text, bits];
            /* A global address first, else the link's. */
            if (d[@"ipv6"] == nil || (!IN6_IS_ADDR_LINKLOCAL(&six->sin6_addr) && [d[@"ipv6"] hasPrefix:@"fe80"]))
                d[@"ipv6"] = value;
        } else if (a->ifa_addr->sa_family == AF_LINK) {
            struct sockaddr_dl *link = (struct sockaddr_dl *)(void *)a->ifa_addr;
            if (link->sdl_alen == 6) {
                const unsigned char *m = (const unsigned char *)LLADDR(link);
                d[@"mac"] = [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]];
            }
        }
    }
    freeifaddrs(list);
    NSMutableArray *rows = [NSMutableArray array];
    for (NSString *name in order) {
        NSDictionary *d = found[name];
        /* The ones a user has: Ethernet and the like, and the loopback. */
        if ([name hasPrefix:@"en"] || [d[@"loopback"] boolValue] || d[@"ipv4"] != nil)
            [rows addObject:d];
    }
    [rows sortUsingComparator:^NSComparisonResult(NSDictionary *x, NSDictionary *y) {
        if ([x[@"loopback"] boolValue] != [y[@"loopback"] boolValue])
            return [x[@"loopback"] boolValue] ? NSOrderedDescending : NSOrderedAscending;
        return [x[@"name"] compare:y[@"name"] options:NSNumericSearch];
    }];
    return rows;
}

/* How the network service on an interface configures IPv4 and IPv6: the
   names the Network pane shows. */
static NSDictionary<NSString *, NSString *> *configMethods(void)
{
    NSMutableDictionary *methods = [NSMutableDictionary dictionary];
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network extra"), NULL);
    NSArray *services = prefs != NULL ? CFBridgingRelease(SCNetworkServiceCopyAll(prefs)) : nil;

    for (id service in services) {
        SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface((__bridge SCNetworkServiceRef)service);
        NSString *name = interface != NULL ? (__bridge NSString *)SCNetworkInterfaceGetBSDName(interface) : nil;
        if (name == nil)
            continue;
        for (NSString *type in @[ @"IPv4", @"IPv6" ]) {
            SCNetworkProtocolRef protocol = SCNetworkServiceCopyProtocol((__bridge SCNetworkServiceRef)service,
                                                                         (__bridge CFStringRef)type);
            NSDictionary *config = protocol != NULL ? (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(protocol) : nil;
            NSString *method = config[@"ConfigMethod"];
            if (method != nil)
                methods[[NSString stringWithFormat:@"%@ %@", name, type]] = method;
            if (protocol != NULL)
                CFRelease(protocol);
        }
    }
    if (prefs != NULL)
        CFRelease(prefs);
    return methods;
}

@implementation NetworkExtra {
    NSStatusItem *_item;
    SCDynamicStoreRef _store;
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Network"];
    [menu setDelegate:self];
    [menu setAutoenablesItems:NO];
    [_item setMenu:menu];

    SCDynamicStoreContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    _store = SCDynamicStoreCreate(NULL, CFSTR("org.minkar.menuextra.network"), networkChanged, &context);
    if (_store != NULL) {
        SCDynamicStoreSetNotificationKeys(_store, (__bridge CFArrayRef)@[ @"State:/Network/Global/IPv4" ],
                                          (__bridge CFArrayRef)@[ @"State:/Network/Interface/.*/IPv4" ]);
        SCDynamicStoreSetDispatchQueue(_store, dispatch_get_main_queue());
    }
    [self update];
    return self;
}

- (void)update
{
    BOOL connected = NO;
    for (NSArray *row in interfacesUp())
        connected |= [row[1] length] > 0;
    NSString *name = connected ? @"network-wired" : @"network-offline";
    NSImage *icon = [[NSImage alloc] initWithContentsOfFile:
        [NSString stringWithFormat:@"/System/Library/MenuBarIcons/%@.png", name]];
    [icon setSize:NSMakeSize(18, 18)];
    /* Black on clear: drawn in the menu bar's own color. */
    [icon setTemplate:YES];
    if (icon != nil)
        [[_item button] setImage:icon];
    else
        [[_item button] setTitle:NSLocalizedString(@"Network", nil)];
}

/* A line of an interface's submenu: "Label: value"; choosing it copies
   the value. */
- (void)addLine:(NSString *)label value:(NSString *)value to:(NSMenu *)menu
{
    NSMenuItem *item = [menu addItemWithTitle:[NSString stringWithFormat:@"%@: %@", label, value]
                                       action:@selector(copyValue:) keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:value];
}

- (NSString *)methodName:(NSString *)method
{
    NSDictionary *names = @{ @"DHCP" : NSLocalizedString(@"DHCP", nil), @"BOOTP" : NSLocalizedString(@"BOOTP", nil),
                             @"Manual" : NSLocalizedString(@"Manual", nil),
                             @"Automatic" : NSLocalizedString(@"Automatic", nil),
                             @"LinkLocal" : NSLocalizedString(@"Link-local only", nil),
                             @"INFORM" : NSLocalizedString(@"DHCP with manual address", nil) };
    return names[method] ?: method;
}

/* Made as it opens: an item for each interface, its details in a submenu. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    NSArray<NSDictionary *> *rows = interfaceDetails();
    NSDictionary *methods = configMethods();

    [menu removeAllItems];
    for (NSDictionary *d in rows) {
        NSString *name = d[@"name"];
        NSMenuItem *item = [menu addItemWithTitle:name action:NULL keyEquivalent:@""];
        NSMenu *details = [[NSMenu alloc] initWithTitle:name];
        [details setAutoenablesItems:NO];

        [self addLine:NSLocalizedString(@"Status", nil)
                value:[d[@"up"] boolValue] ? NSLocalizedString(@"Up", nil) : NSLocalizedString(@"Down", nil)
                   to:details];
        if (d[@"ipv4"] != nil) {
            [self addLine:NSLocalizedString(@"IPv4", nil) value:d[@"ipv4"] to:details];
            NSString *method = methods[[NSString stringWithFormat:@"%@ IPv4", name]];
            if (method != nil)
                [self addLine:NSLocalizedString(@"IPv4 Mode", nil) value:[self methodName:method] to:details];
        }
        if (d[@"ipv6"] != nil) {
            [self addLine:NSLocalizedString(@"IPv6", nil) value:d[@"ipv6"] to:details];
            NSString *method = methods[[NSString stringWithFormat:@"%@ IPv6", name]];
            if (method != nil)
                [self addLine:NSLocalizedString(@"IPv6 Mode", nil) value:[self methodName:method] to:details];
        }
        if (d[@"mac"] != nil)
            [self addLine:NSLocalizedString(@"MAC Address", nil) value:d[@"mac"] to:details];
        [item setSubmenu:details];
    }
    if ([rows count] == 0)
        [[menu addItemWithTitle:NSLocalizedString(@"No network interfaces", nil) action:NULL keyEquivalent:@""] setEnabled:NO];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *prefs = [menu addItemWithTitle:NSLocalizedString(@"Network Preferences…", nil)
                                        action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
}

- (void)copyValue:(NSMenuItem *)sender
{
    NSPasteboard *board = [NSPasteboard generalPasteboard];

    [board clearContents];
    [board setString:[sender representedObject] forType:NSPasteboardTypeString];
}

- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Network.prefPane"]];
}

@end
