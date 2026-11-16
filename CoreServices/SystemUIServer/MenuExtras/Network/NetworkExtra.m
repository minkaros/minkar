/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network Menu Extra (Atlas.md, MB1): its icon says whether a wired
 * interface is connected; its menu lists the interfaces that are up with
 * their IPv4 addresses, and Network Preferences opens the pane. It looks
 * again when the system's network configuration changes
 * (SystemConfiguration's State:/Network/Global/IPv4), not on a timer.
 */

#import "MenuExtra.h"
#include <SystemConfiguration/SystemConfiguration.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

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
        [NSString stringWithFormat:@"/System/Library/Icons/menubar/%@.png", name]];
    [icon setSize:NSMakeSize(18, 18)];
    if (icon != nil)
        [[_item button] setImage:icon];
    else
        [[_item button] setTitle:NSLocalizedString(@"Network", nil)];
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
    NSArray *rows = interfacesUp();

    [menu removeAllItems];
    for (NSArray<NSString *> *row in rows) {
        NSString *title = [row[1] length] > 0 ? [NSString stringWithFormat:@"%@: %@", row[0], row[1]]
                                              : [NSString stringWithFormat:@"%@: %@", row[0], NSLocalizedString(@"Not connected", nil)];
        [[menu addItemWithTitle:title action:NULL keyEquivalent:@""] setEnabled:NO];
    }
    if ([rows count] == 0)
        [[menu addItemWithTitle:NSLocalizedString(@"No network interfaces", nil) action:NULL keyEquivalent:@""] setEnabled:NO];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *prefs = [menu addItemWithTitle:NSLocalizedString(@"Network Preferences…", nil)
                                        action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
}

- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Network.prefPane"]];
}

@end
