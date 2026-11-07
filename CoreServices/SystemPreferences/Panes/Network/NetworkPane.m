/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network pane (ROADMAP Phase 5, item 6): the current location's
 * services, each with its state (connected: an address in configd's
 * State:/Network/Service/<id>/IPv4); the one chosen shows its IPv4
 * configuration, Using DHCP or Manually (address, subnet mask, router) and
 * its DNS servers, and Apply sets them through SCPreferences, as
 * networksetup does. Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <os/log.h>

@interface NetworkPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 300, kListWidth = 190, kRow = 40;

static NSTextField *label(NSString *text, NSRect frame, BOOL right)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:right ? NSRightTextAlignment : NSLeftTextAlignment];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

static NSTextField *entry(NSRect frame)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setEditable:YES];
    [field setBezeled:YES];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

/* ---- The services' list ---- */

@class NetworkListView;
@protocol NetworkListDelegate <NSObject>
- (void)list:(NetworkListView *)list chose:(NSUInteger)index;
@end

@interface NetworkListView : NSView
@property (nonatomic, copy) NSArray<NSDictionary *> *rows;   /* name, connected, status */
@property (nonatomic) NSInteger selected;
@property (nonatomic, weak) id<NetworkListDelegate> delegate;
@end

@implementation NetworkListView

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)dirty
{
    NSDictionary *name = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                            NSForegroundColorAttributeName: [NSColor blackColor] };
    NSDictionary *status = @{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                              NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.4 alpha:1] };

    [[NSColor whiteColor] set];
    NSRectFill([self bounds]);
    [[NSColor colorWithCalibratedWhite:0.75 alpha:1] set];
    NSFrameRect([self bounds]);
    for (NSUInteger i = 0; i < [self.rows count]; i++) {
        NSRect row = NSMakeRect(1, i * kRow + 1, NSWidth([self bounds]) - 2, kRow);
        if ((NSInteger)i == self.selected) {
            [[NSColor colorWithCalibratedRed:0.80 green:0.87 blue:0.97 alpha:1] set];
            NSRectFill(row);
        }
        BOOL connected = [self.rows[i][@"connected"] boolValue];
        [(connected ? [NSColor colorWithCalibratedRed:0.2 green:0.75 blue:0.3 alpha:1]
                    : [NSColor colorWithCalibratedRed:0.85 green:0.25 blue:0.2 alpha:1]) setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(10, NSMinY(row) + 15, 10, 10)] fill];
        [self.rows[i][@"name"] drawAtPoint:NSMakePoint(28, NSMinY(row) + 4) withAttributes:name];
        [self.rows[i][@"status"] drawAtPoint:NSMakePoint(28, NSMinY(row) + 22) withAttributes:status];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger i = (NSInteger)(p.y / kRow);

    if (i >= 0 && (NSUInteger)i < [self.rows count]) {
        self.selected = i;
        [self setNeedsDisplay:YES];
        [self.delegate list:self chose:(NSUInteger)i];
    }
}

@end

/* ---- The pane ---- */

@interface NetworkPane () <NetworkListDelegate>
@end

@implementation NetworkPane {
    NetworkListView *_list;
    NSArray *_serviceIDs;
    NSArray *_serviceNames;
    NSTextField *_status, *_address, *_mask, *_router, *_dns;
    NSPopUpButton *_method;
    NSButton *_apply;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

/* The current location's services, in their order. */
- (void)loadServices
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkSetRef set = prefs ? SCNetworkSetCopyCurrent(prefs) : NULL;
    NSArray *services = set ? (__bridge_transfer NSArray *)SCNetworkSetCopyServices(set) : @[];
    NSMutableArray *ids = [NSMutableArray array], *names = [NSMutableArray array], *rows = [NSMutableArray array];

    for (id s in services) {
        SCNetworkServiceRef service = (__bridge SCNetworkServiceRef)s;
        NSString *sid = (__bridge NSString *)SCNetworkServiceGetServiceID(service);
        NSString *name = (__bridge NSString *)SCNetworkServiceGetName(service);
        NSDictionary *state = [self stateOf:sid];
        BOOL connected = [state[@"Addresses"] count] > 0;
        [ids addObject:sid];
        [names addObject:name ?: sid];
        [rows addObject:@{ @"name": name ?: sid, @"connected": @(connected),
                           @"status": connected ? [self text:@"Connected"] : [self text:@"Not Connected"] }];
    }
    _serviceIDs = ids;
    _serviceNames = names;
    _list.rows = rows;
    if (set)
        CFRelease(set);
    if (prefs)
        CFRelease(prefs);
}

/* What configd has for the service now (its IPv4 state). */
- (NSDictionary *)stateOf:(NSString *)serviceID
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("Network pane"), NULL, NULL);
    CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState,
        (__bridge CFStringRef)serviceID, kSCEntNetIPv4);
    NSDictionary *state = store ? (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(store, key) : nil;

    CFRelease(key);
    if (store)
        CFRelease(store);
    return [state isKindOfClass:[NSDictionary class]] ? state : @{};
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    CGFloat x = kListWidth + 40;

    _list = [[NetworkListView alloc] initWithFrame:NSMakeRect(20, 20, kListWidth, kHeight - 40)];
    _list.delegate = self;
    [view addSubview:_list];

    _status = label(@"", NSMakeRect(x, 250, 400, 20), NO);
    [_status setFont:[NSFont boldSystemFontOfSize:13]];
    [view addSubview:_status];

    [view addSubview:label([self text:@"Configure IPv4:"], NSMakeRect(x, 210, 130, 20), YES)];
    _method = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x + 140, 206, 200, 26) pullsDown:NO];
    [_method addItemWithTitle:[self text:@"Using DHCP"]];
    [_method addItemWithTitle:[self text:@"Manually"]];
    [_method setTarget:self];
    [_method setAction:@selector(chooseMethod:)];
    [view addSubview:_method];

    NSArray *names = @[ @"IP Address:", @"Subnet Mask:", @"Router:", @"DNS Server:" ];
    NSMutableArray *fields = [NSMutableArray array];
    for (NSUInteger i = 0; i < [names count]; i++) {
        CGFloat y = 170 - i * 34;
        [view addSubview:label([self text:names[i]], NSMakeRect(x, y, 130, 20), YES)];
        NSTextField *f = entry(NSMakeRect(x + 140, y - 2, 220, 24));
        [view addSubview:f];
        [fields addObject:f];
    }
    _address = fields[0];
    _mask = fields[1];
    _router = fields[2];
    _dns = fields[3];

    _apply = [[NSButton alloc] initWithFrame:NSMakeRect(kWidth - 120, 20, 96, 28)];
    [_apply setTitle:[self text:@"Apply"]];
    [_apply setBezelStyle:NSRoundedBezelStyle];
    [_apply setTarget:self];
    [_apply setAction:@selector(apply:)];
    [view addSubview:_apply];

    [self loadServices];
    _list.selected = [_serviceIDs count] ? 0 : -1;
    if ([_serviceIDs count])
        [self list:_list chose:0];
    [self setMainView:view];
    return view;
}

static NSString *first(id array)
{
    return [array isKindOfClass:[NSArray class]] && [array count] ? [array firstObject] : @"";
}

/* The chosen service: its configuration (Setup:) and its state now. */
- (void)list:(NetworkListView *)list chose:(NSUInteger)index
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkServiceRef service = prefs ? SCNetworkServiceCopy(prefs, (__bridge CFStringRef)_serviceIDs[index]) : NULL;
    SCNetworkProtocolRef ipv4 = service ? SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeIPv4) : NULL;
    SCNetworkProtocolRef dnsp = service ? SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeDNS) : NULL;
    NSDictionary *config = ipv4 ? (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(ipv4) : nil;
    NSDictionary *dns = dnsp ? (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(dnsp) : nil;
    NSDictionary *state = [self stateOf:_serviceIDs[index]];
    BOOL manual = [config[(__bridge NSString *)kSCPropNetIPv4ConfigMethod]
                   isEqual:(__bridge NSString *)kSCValNetIPv4ConfigMethodManual];

    [_method selectItemAtIndex:manual ? 1 : 0];
    NSString *address = first(state[@"Addresses"]);
    [_status setStringValue:[address length] ?
        [NSString stringWithFormat:[self text:@"%@ is connected and has the IP address %@."], _serviceNames[index], address] :
        [NSString stringWithFormat:[self text:@"%@ is not connected."], _serviceNames[index]]];
    NSDictionary *source = manual ? config : state;
    [_address setStringValue:first(source[@"Addresses"])];
    [_mask setStringValue:first(source[@"SubnetMasks"])];
    id router = source[@"Router"];
    [_router setStringValue:[router isKindOfClass:[NSString class]] ? router : @""];
    [_dns setStringValue:[first(dns[(__bridge NSString *)kSCPropNetDNSServerAddresses]) description] ?: @""];
    [self chooseMethod:nil];

    if (dnsp)
        CFRelease(dnsp);
    if (ipv4)
        CFRelease(ipv4);
    if (service)
        CFRelease(service);
    if (prefs)
        CFRelease(prefs);
}

/* Manually: the address fields can be typed in. */
- (void)chooseMethod:(id)sender
{
    BOOL manual = [_method indexOfSelectedItem] == 1;

    for (NSTextField *f in @[ _address, _mask, _router ])
        [f setEditable:manual];
}

- (void)apply:(id)sender
{
    NSInteger index = _list.selected;
    if (index < 0)
        return;
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkServiceRef service = prefs ? SCNetworkServiceCopy(prefs, (__bridge CFStringRef)_serviceIDs[(NSUInteger)index]) : NULL;
    BOOL ok = service != NULL;
    BOOL manual = [_method indexOfSelectedItem] == 1;

    if (ok) {
        NSMutableDictionary *config = [NSMutableDictionary dictionary];
        config[(__bridge NSString *)kSCPropNetIPv4ConfigMethod] = manual ?
            (__bridge NSString *)kSCValNetIPv4ConfigMethodManual : (__bridge NSString *)kSCValNetIPv4ConfigMethodDHCP;
        if (manual) {
            config[(__bridge NSString *)kSCPropNetIPv4Addresses] = @[ [_address stringValue] ];
            config[(__bridge NSString *)kSCPropNetIPv4SubnetMasks] = @[ [_mask stringValue] ];
            if ([[_router stringValue] length])
                config[(__bridge NSString *)kSCPropNetIPv4Router] = [_router stringValue];
        }
        ok = [self setProtocol:kSCNetworkProtocolTypeIPv4 of:service to:config];
        NSString *dns = [[_dns stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if (ok)
            ok = [self setProtocol:kSCNetworkProtocolTypeDNS of:service
                                to:[dns length] ? @{ (__bridge NSString *)kSCPropNetDNSServerAddresses:
                                                      [dns componentsSeparatedByString:@" "] } : @{}];
        ok = ok && SCPreferencesCommitChanges(prefs) && SCPreferencesApplyChanges(prefs);
    }
    os_log_info(OS_LOG_DEFAULT, "network %{public}s %{public}s: %d", [_serviceNames[(NSUInteger)index] UTF8String],
                manual ? "manual" : "dhcp", ok);
    if (!ok)
        NSBeep();
    if (service)
        CFRelease(service);
    if (prefs)
        CFRelease(prefs);
}

- (BOOL)setProtocol:(CFStringRef)type of:(SCNetworkServiceRef)service to:(NSDictionary *)config
{
    SCNetworkProtocolRef p = SCNetworkServiceCopyProtocol(service, type);
    BOOL ok;

    if (p == NULL && (!SCNetworkServiceAddProtocolType(service, type) ||
                      (p = SCNetworkServiceCopyProtocol(service, type)) == NULL))
        return NO;
    ok = SCNetworkProtocolSetConfiguration(p, (__bridge CFDictionaryRef)config) &&
         SCNetworkProtocolSetEnabled(p, true);
    CFRelease(p);
    return ok;
}

@end
