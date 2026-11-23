/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network pane, laid out as the reference's: the Location on top
 * (the network sets; choosing one makes it current); on the left the
 * current location's services, each with its state, and below them the
 * buttons that add a service (for an interface that has none) and delete
 * the chosen one; on the right the chosen service's Status (a dot and a
 * sentence), Configure IPv4 (Using DHCP, Using DHCP with manual address,
 * Using BootP, Manually, Off), its address, subnet mask and router, its
 * DNS servers and search domains (configd's, in grey, when it has none of
 * its own) and its IPv6 address; Revert and Apply. Read and written
 * through SystemConfiguration, as networksetup does. The view is made in
 * code, in the system's colors.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <os/log.h>
#import "NetworkAdvanced.h"

@interface NetworkPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 420, kListWidth = 200, kRow = 40;

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

static NSColor *green(void)
{
    return [NSColor colorWithSRGBRed:0.2 green:0.75 blue:0.3 alpha:1];
}

static NSColor *red(void)
{
    return [NSColor colorWithSRGBRed:0.85 green:0.25 blue:0.2 alpha:1];
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
    [[NSColor controlBackgroundColor] set];
    NSRectFill([self bounds]);
    [[NSColor gridColor] set];
    NSFrameRect([self bounds]);
    for (NSUInteger i = 0; i < [self.rows count]; i++) {
        NSRect row = NSMakeRect(1, i * kRow + 1, NSWidth([self bounds]) - 2, kRow);
        BOOL chosen = (NSInteger)i == self.selected;
        NSColor *text = chosen ? [NSColor alternateSelectedControlTextColor] : [NSColor labelColor];
        NSColor *detail = chosen ? [NSColor alternateSelectedControlTextColor] : [NSColor secondaryLabelColor];

        if (chosen) {
            [[NSColor selectedContentBackgroundColor] set];
            NSRectFill(row);
        }
        BOOL connected = [self.rows[i][@"connected"] boolValue];
        [(connected ? green() : red()) setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(10, NSMinY(row) + 15, 10, 10)] fill];
        [self.rows[i][@"name"] drawAtPoint:NSMakePoint(28, NSMinY(row) + 4)
                            withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                                              NSForegroundColorAttributeName: text }];
        [self.rows[i][@"status"] drawAtPoint:NSMakePoint(28, NSMinY(row) + 22)
                              withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                                                NSForegroundColorAttributeName: detail }];
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

/* The status dot beside "Status:". */
@interface NetworkDotView : NSView
@property (nonatomic, strong) NSColor *color;
@end

@implementation NetworkDotView
- (void)drawRect:(NSRect)dirty
{
    [self.color ?: red() setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(self.bounds, 1, 1)] fill];
}
@end

/* ---- The pane ---- */

/* Configure IPv4's choices, in the reference's order, and their methods. */
static NSString *const kMethods[] = { @"DHCP", @"INFORM", @"BOOTP", @"Manual", @"Off" };
static const NSUInteger kMethodCount = 5;

@interface NetworkPane () <NetworkListDelegate>
@end

@implementation NetworkPane {
    NetworkListView *_list;
    NSPopUpButton *_location, *_method;
    NSArray *_setIDs;
    NSArray *_serviceIDs;
    NSArray *_serviceNames;
    NSTextField *_statusValue, *_statusText, *_address, *_mask, *_router, *_dns, *_domains, *_ipv6;
    NetworkDotView *_dot;
    NSButton *_add, *_remove, *_revert, *_apply, *_advanced;
    SCPreferencesRef _editing;      /* what Apply commits (the Advanced sheet's changes too) */
    NetworkAdvanced *_sheet;
}

/* The preferences being edited: made when first needed, committed by
   Apply, dropped by Revert. */
- (SCPreferencesRef)editing
{
    if (_editing == NULL)
        _editing = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    return _editing;
}

- (void)dropEditing
{
    if (_editing != NULL)
        CFRelease(_editing);
    _editing = NULL;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

/* What configd has for a service now, one entity (IPv4, IPv6, DNS). */
- (NSDictionary *)state:(NSString *)entity of:(NSString *)serviceID
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("Network pane"), NULL, NULL);
    CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState,
        (__bridge CFStringRef)serviceID, (__bridge CFStringRef)entity);
    NSDictionary *state = store ? (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(store, key) : nil;

    CFRelease(key);
    if (store)
        CFRelease(store);
    return [state isKindOfClass:[NSDictionary class]] ? state : @{};
}

/* The system's DNS now (State:/Network/Global/DNS). */
- (NSDictionary *)globalDNS
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("Network pane"), NULL, NULL);
    NSDictionary *dns = store ? (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(store,
        CFSTR("State:/Network/Global/DNS")) : nil;

    if (store)
        CFRelease(store);
    return [dns isKindOfClass:[NSDictionary class]] ? dns : @{};
}

/* The locations (network sets), the current one chosen. */
- (void)loadLocations
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    NSArray *sets = prefs ? (__bridge_transfer NSArray *)SCNetworkSetCopyAll(prefs) : @[];
    SCNetworkSetRef current = prefs ? SCNetworkSetCopyCurrent(prefs) : NULL;
    NSString *currentID = current ? (__bridge NSString *)SCNetworkSetGetSetID(current) : nil;
    NSMutableArray *ids = [NSMutableArray array];

    [_location removeAllItems];
    for (id s in sets) {
        NSString *sid = (__bridge NSString *)SCNetworkSetGetSetID((__bridge SCNetworkSetRef)s);
        NSString *name = (__bridge NSString *)SCNetworkSetGetName((__bridge SCNetworkSetRef)s);
        [_location addItemWithTitle:name ?: [self text:@"Automatic"]];
        [ids addObject:sid];
        if ([sid isEqual:currentID])
            [_location selectItemAtIndex:[ids count] - 1];
    }
    if ([ids count] == 0)
        [_location addItemWithTitle:[self text:@"Automatic"]];
    [_location setEnabled:[ids count] > 1];
    _setIDs = ids;
    if (current)
        CFRelease(current);
    if (prefs)
        CFRelease(prefs);
}

/* The current location's services, in their order. */
- (void)loadServices
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkSetRef set = prefs ? SCNetworkSetCopyCurrent(prefs) : NULL;
    NSArray *services = set ? (__bridge_transfer NSArray *)SCNetworkSetCopyServices(set) : @[];
    NSMutableArray *ids = [NSMutableArray array], *names = [NSMutableArray array], *rows = [NSMutableArray array];

    /* In the location's service order, as the reference lists them (the
       set's services come in no order of their own). */
    NSArray *order = set ? (__bridge NSArray *)SCNetworkSetGetServiceOrder(set) : nil;
    services = [services sortedArrayUsingComparator:^NSComparisonResult(id x, id y) {
        NSUInteger a = [order indexOfObject:(__bridge NSString *)SCNetworkServiceGetServiceID((__bridge SCNetworkServiceRef)x)];
        NSUInteger b = [order indexOfObject:(__bridge NSString *)SCNetworkServiceGetServiceID((__bridge SCNetworkServiceRef)y)];
        if (a != b)
            return a < b ? NSOrderedAscending : NSOrderedDescending;
        return [(__bridge NSString *)SCNetworkServiceGetName((__bridge SCNetworkServiceRef)x)
                   compare:(__bridge NSString *)SCNetworkServiceGetName((__bridge SCNetworkServiceRef)y)
                   options:NSNumericSearch];
    }];

    for (id s in services) {
        SCNetworkServiceRef service = (__bridge SCNetworkServiceRef)s;
        NSString *sid = (__bridge NSString *)SCNetworkServiceGetServiceID(service);
        NSString *name = (__bridge NSString *)SCNetworkServiceGetName(service);
        BOOL connected = [[self state:@"IPv4" of:sid][@"Addresses"] count] > 0;
        [ids addObject:sid];
        [names addObject:name ?: sid];
        [rows addObject:@{ @"name": name ?: sid, @"connected": @(connected),
                           @"status": connected ? [self text:@"Connected"] : [self text:@"Not Connected"] }];
    }
    _serviceIDs = ids;
    _serviceNames = names;
    _list.rows = rows;
    [_list setNeedsDisplay:YES];
    [_remove setEnabled:[ids count] > 0];
    if (set)
        CFRelease(set);
    if (prefs)
        CFRelease(prefs);
}

- (NSTextField *)row:(NSString *)name y:(CGFloat)y in:(NSView *)view x:(CGFloat)x
{
    [view addSubview:label([self text:name], NSMakeRect(x, y, 130, 20), YES)];
    NSTextField *f = entry(NSMakeRect(x + 140, y - 2, 240, 24));
    [view addSubview:f];
    return f;
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    CGFloat x = kListWidth + 30;

    /* Location, centered on top. */
    [view addSubview:label([self text:@"Location:"], NSMakeRect(150, kHeight - 38, 120, 20), YES)];
    _location = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(278, kHeight - 42, 220, 26) pullsDown:NO];
    [_location setTarget:self];
    [_location setAction:@selector(chooseLocation:)];
    [view addSubview:_location];

    _list = [[NetworkListView alloc] initWithFrame:NSMakeRect(20, 60, kListWidth, kHeight - 120)];
    _list.delegate = self;
    [view addSubview:_list];
    _add = [[NSButton alloc] initWithFrame:NSMakeRect(20, 34, 26, 24)];
    [_add setTitle:@"+"];
    [_add setTarget:self];
    [_add setAction:@selector(addService:)];
    _remove = [[NSButton alloc] initWithFrame:NSMakeRect(45, 34, 26, 24)];
    [_remove setTitle:@"−"];
    [_remove setTarget:self];
    [_remove setAction:@selector(removeService:)];
    [view addSubview:_add];
    [view addSubview:_remove];

    /* Status: a dot, a word, a sentence. */
    CGFloat y = kHeight - 90;
    [view addSubview:label([self text:@"Status:"], NSMakeRect(x, y, 130, 20), YES)];
    _dot = [[NetworkDotView alloc] initWithFrame:NSMakeRect(x + 140, y + 4, 12, 12)];
    [view addSubview:_dot];
    _statusValue = label(@"", NSMakeRect(x + 158, y, 240, 20), NO);
    [_statusValue setFont:[NSFont boldSystemFontOfSize:13]];
    [view addSubview:_statusValue];
    _statusText = label(@"", NSMakeRect(x + 158, y - 34, 250, 32), NO);
    [_statusText setFont:[NSFont systemFontOfSize:11]];
    [_statusText setTextColor:[NSColor secondaryLabelColor]];
    [view addSubview:_statusText];

    y -= 70;
    [view addSubview:label([self text:@"Configure IPv4:"], NSMakeRect(x, y, 130, 20), YES)];
    _method = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x + 140, y - 4, 240, 26) pullsDown:NO];
    for (NSString *title in @[ @"Using DHCP", @"Using DHCP with manual address", @"Using BootP", @"Manually", @"Off" ])
        [_method addItemWithTitle:[self text:title]];
    [_method setTarget:self];
    [_method setAction:@selector(chooseMethod:)];
    [view addSubview:_method];

    y -= 34;
    _address = [self row:@"IP Address:" y:y in:view x:x];
    _mask = [self row:@"Subnet Mask:" y:y - 30 in:view x:x];
    _router = [self row:@"Router:" y:y - 60 in:view x:x];
    _dns = [self row:@"DNS Server:" y:y - 98 in:view x:x];
    _domains = [self row:@"Search Domains:" y:y - 128 in:view x:x];
    [view addSubview:label([self text:@"IPv6 Address:"], NSMakeRect(x, y - 166, 130, 20), YES)];
    _ipv6 = label(@"", NSMakeRect(x + 140, y - 166, 260, 20), NO);
    [_ipv6 setSelectable:YES];
    [view addSubview:_ipv6];

    _advanced = [[NSButton alloc] initWithFrame:NSMakeRect(kWidth - 336, 14, 110, 28)];
    [_advanced setTitle:[self text:@"Advanced\u2026"]];
    [_advanced setBezelStyle:NSRoundedBezelStyle];
    [_advanced setTarget:self];
    [_advanced setAction:@selector(advanced:)];
    [view addSubview:_advanced];

    _revert = [[NSButton alloc] initWithFrame:NSMakeRect(kWidth - 220, 14, 96, 28)];
    [_revert setTitle:[self text:@"Revert"]];
    [_revert setBezelStyle:NSRoundedBezelStyle];
    [_revert setTarget:self];
    [_revert setAction:@selector(revert:)];
    [view addSubview:_revert];
    _apply = [[NSButton alloc] initWithFrame:NSMakeRect(kWidth - 120, 14, 96, 28)];
    [_apply setTitle:[self text:@"Apply"]];
    [_apply setBezelStyle:NSRoundedBezelStyle];
    [_apply setKeyEquivalent:@"\r"];
    [_apply setTarget:self];
    [_apply setAction:@selector(apply:)];
    [view addSubview:_apply];

    [self loadLocations];
    [self loadServices];
    _list.selected = [_serviceIDs count] ? 0 : -1;
    if ([_serviceIDs count])
        [self list:_list chose:0];
    [self setMainView:view];
    return view;
}

static NSString *first(id array)
{
    return [array isKindOfClass:[NSArray class]] && [array count] ? [[array firstObject] description] : @"";
}

static NSString *joined(id array)
{
    return [array isKindOfClass:[NSArray class]] ? [array componentsJoinedByString:@", "] : @"";
}

/* The chosen service: its configuration (Setup:) and its state now. */
- (void)list:(NetworkListView *)list chose:(NSUInteger)index
{
    SCPreferencesRef prefs = (SCPreferencesRef)CFRetain([self editing]);
    SCNetworkServiceRef service = prefs ? SCNetworkServiceCopy(prefs, (__bridge CFStringRef)_serviceIDs[index]) : NULL;
    SCNetworkProtocolRef ipv4 = service ? SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeIPv4) : NULL;
    SCNetworkProtocolRef dnsp = service ? SCNetworkServiceCopyProtocol(service, kSCNetworkProtocolTypeDNS) : NULL;
    NSDictionary *config = ipv4 ? (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(ipv4) : nil;
    NSDictionary *dns = dnsp ? (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(dnsp) : nil;
    NSString *sid = _serviceIDs[index], *name = _serviceNames[index];
    NSDictionary *state = [self state:@"IPv4" of:sid], *state6 = [self state:@"IPv6" of:sid];
    NSString *method = config[@"ConfigMethod"] ?: @"DHCP";
    NSUInteger choice = 0;

    for (NSUInteger i = 0; i < kMethodCount; i++)
        if ([kMethods[i] isEqual:method])
            choice = i;
    [_method selectItemAtIndex:choice];

    NSString *address = first(state[@"Addresses"]);
    BOOL connected = [address length] > 0;
    _dot.color = connected ? green() : red();
    [_dot setNeedsDisplay:YES];
    [_statusValue setStringValue:connected ? [self text:@"Connected"] : [self text:@"Not Connected"]];
    [_statusText setStringValue:connected ?
        [NSString stringWithFormat:[self text:@"%@ is currently active and has the IP address %@."], name, address] :
        [NSString stringWithFormat:[self text:@"%@ is not connected."], name]];

    /* Set by hand: the configuration's; else what the service has now. */
    BOOL ownAddress = [method isEqual:@"Manual"] || [method isEqual:@"INFORM"];
    BOOL ownMask = [method isEqual:@"Manual"];
    [_address setStringValue:first((ownAddress ? config : state)[@"Addresses"])];
    [_mask setStringValue:first((ownMask ? config : state)[@"SubnetMasks"])];
    id router = (ownMask ? config : state)[@"Router"];
    [_router setStringValue:[router isKindOfClass:[NSString class]] ? router : @""];

    /* DNS: the service's own, typed in; configd's (from DHCP) shown in
       grey where it has none, as on the reference. */
    NSDictionary *now = [self state:@"DNS" of:sid];
    if ([now count] == 0)
        now = [self globalDNS];
    [_dns setStringValue:[dns[@"ServerAddresses"] isKindOfClass:[NSArray class]] ?
        [dns[@"ServerAddresses"] componentsJoinedByString:@" "] : @""];
    [[_dns cell] setPlaceholderString:joined(now[@"ServerAddresses"])];
    [_domains setStringValue:[dns[@"SearchDomains"] isKindOfClass:[NSArray class]] ?
        [dns[@"SearchDomains"] componentsJoinedByString:@" "] : @""];
    [[_domains cell] setPlaceholderString:joined(now[@"SearchDomains"])];

    NSArray *v6 = state6[@"Addresses"];
    NSArray *prefixes = state6[@"PrefixLength"];
    [_ipv6 setStringValue:[v6 count] ? ([prefixes count] ? [NSString stringWithFormat:@"%@/%@", v6[0], prefixes[0]]
                                                            : first(v6)) : @""];
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

/* Which fields can be typed in: Manually all three, DHCP with a manual
   address the address alone, the others none. */
- (void)chooseMethod:(id)sender
{
    NSString *method = kMethods[MAX(0, [_method indexOfSelectedItem])];
    BOOL manual = [method isEqual:@"Manual"], inform = [method isEqual:@"INFORM"];

    [_address setEditable:manual || inform];
    [_mask setEditable:manual];
    [_router setEditable:manual];
    for (NSTextField *f in @[ _address, _mask, _router ]) {
        [f setTextColor:[f isEditable] ? [NSColor controlTextColor] : [NSColor secondaryLabelColor]];
        [f setNeedsDisplay:YES];
    }
}

- (void)chooseLocation:(id)sender
{
    NSInteger i = [_location indexOfSelectedItem];
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkSetRef set = (prefs && i >= 0 && (NSUInteger)i < [_setIDs count]) ?
        SCNetworkSetCopy(prefs, (__bridge CFStringRef)_setIDs[(NSUInteger)i]) : NULL;
    BOOL ok = set != NULL && SCNetworkSetSetCurrent(set) && SCPreferencesCommitChanges(prefs) &&
              SCPreferencesApplyChanges(prefs);

    if (!ok)
        NSBeep();
    if (set)
        CFRelease(set);
    if (prefs)
        CFRelease(prefs);
    [self loadServices];
    _list.selected = [_serviceIDs count] ? 0 : -1;
    if ([_serviceIDs count])
        [self list:_list chose:0];
}

/* Advanced…: the chosen service's sheet, writing into what Apply commits. */
- (void)advanced:(id)sender
{
    NSInteger index = _list.selected;
    if (index < 0 || (NSUInteger)index >= [_serviceIDs count])
        return;
    _sheet = [[NetworkAdvanced alloc] initWithPreferences:[self editing] serviceID:_serviceIDs[(NSUInteger)index]
                                                   bundle:[self bundle]];
    __weak NetworkPane *weakSelf = self;
    [_sheet beginSheetForWindow:[[self mainView] window] completion:^(BOOL changed) {
        NetworkPane *pane = weakSelf;
        if (pane == nil)
            return;
        if (changed)
            [pane list:pane->_list chose:(NSUInteger)index];
        /* Let go once the sheet's own method has returned. */
        dispatch_async(dispatch_get_main_queue(), ^{ pane->_sheet = nil; });
    }];
}

/* The chosen service as it is set up now: what was typed is dropped. */
- (void)revert:(id)sender
{
    [self dropEditing];
    if (_list.selected >= 0)
        [self list:_list chose:(NSUInteger)_list.selected];
}

/* +: a menu of the interfaces with no service in this location; the one
   chosen gets one, set up as its kind is by default. */
- (void)addService:(NSButton *)sender
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkSetRef set = prefs ? SCNetworkSetCopyCurrent(prefs) : NULL;
    NSArray *services = set ? (__bridge_transfer NSArray *)SCNetworkSetCopyServices(set) : @[];
    NSMutableSet *taken = [NSMutableSet set];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Interfaces"];

    for (id s in services) {
        SCNetworkInterfaceRef i = SCNetworkServiceGetInterface((__bridge SCNetworkServiceRef)s);
        NSString *bsd = i ? (__bridge NSString *)SCNetworkInterfaceGetBSDName(i) : nil;
        if (bsd)
            [taken addObject:bsd];
    }
    NSArray *interfaces = (__bridge_transfer NSArray *)SCNetworkInterfaceCopyAll();
    for (id i in interfaces) {
        NSString *bsd = (__bridge NSString *)SCNetworkInterfaceGetBSDName((__bridge SCNetworkInterfaceRef)i);
        if (bsd == nil || [taken containsObject:bsd])
            continue;
        NSString *kind = (__bridge NSString *)SCNetworkInterfaceGetLocalizedDisplayName((__bridge SCNetworkInterfaceRef)i);
        NSMenuItem *item = [menu addItemWithTitle:[NSString stringWithFormat:@"%@ (%@)", kind ?: bsd, bsd]
                                           action:@selector(addInterface:) keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:bsd];
    }
    if ([menu numberOfItems] == 0)
        [[menu addItemWithTitle:[self text:@"Every interface has a service"] action:NULL keyEquivalent:@""] setEnabled:NO];
    [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, NSHeight([sender bounds]) + 2) inView:sender];
    if (set)
        CFRelease(set);
    if (prefs)
        CFRelease(prefs);
}

- (void)addInterface:(NSMenuItem *)sender
{
    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkSetRef set = prefs ? SCNetworkSetCopyCurrent(prefs) : NULL;
    SCNetworkServiceRef service = NULL;
    BOOL ok = NO;

    for (id i in (__bridge_transfer NSArray *)SCNetworkInterfaceCopyAll()) {
        if (![(__bridge NSString *)SCNetworkInterfaceGetBSDName((__bridge SCNetworkInterfaceRef)i)
                isEqual:[sender representedObject]])
            continue;
        service = prefs ? SCNetworkServiceCreate(prefs, (__bridge SCNetworkInterfaceRef)i) : NULL;
        ok = service != NULL && SCNetworkServiceEstablishDefaultConfiguration(service) &&
             set != NULL && SCNetworkSetAddService(set, service) &&
             SCPreferencesCommitChanges(prefs) && SCPreferencesApplyChanges(prefs);
        break;
    }
    os_log_info(OS_LOG_DEFAULT, "network add %{public}s: %d", [[sender representedObject] UTF8String], ok);
    if (!ok)
        NSBeep();
    if (service)
        CFRelease(service);
    if (set)
        CFRelease(set);
    if (prefs)
        CFRelease(prefs);
    [self loadServices];
    if (_list.selected >= (NSInteger)[_serviceIDs count])
        _list.selected = (NSInteger)[_serviceIDs count] - 1;
}

/* -: the chosen service goes, once confirmed. */
- (void)removeService:(id)sender
{
    NSInteger index = _list.selected;
    if (index < 0 || (NSUInteger)index >= [_serviceIDs count])
        return;
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:[self text:@"Delete the service “%@”?"],
                                                    _serviceNames[(NSUInteger)index]]];
    [alert addButtonWithTitle:[self text:@"Delete"]];
    [alert addButtonWithTitle:[self text:@"Cancel"]];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;

    SCPreferencesRef prefs = SCPreferencesCreate(NULL, CFSTR("Network pane"), NULL);
    SCNetworkServiceRef service = prefs ? SCNetworkServiceCopy(prefs, (__bridge CFStringRef)_serviceIDs[(NSUInteger)index]) : NULL;
    BOOL ok = service != NULL && SCNetworkServiceRemove(service) && SCPreferencesCommitChanges(prefs) &&
              SCPreferencesApplyChanges(prefs);

    os_log_info(OS_LOG_DEFAULT, "network remove %{public}s: %d", [_serviceNames[(NSUInteger)index] UTF8String], ok);
    if (!ok)
        NSBeep();
    if (service)
        CFRelease(service);
    if (prefs)
        CFRelease(prefs);
    [self loadServices];
    _list.selected = [_serviceIDs count] ? 0 : -1;
    if ([_serviceIDs count])
        [self list:_list chose:0];
}

static NSArray *words(NSTextField *field)
{
    NSMutableArray *list = [NSMutableArray array];
    for (NSString *w in [[field stringValue] componentsSeparatedByCharactersInSet:
                            [NSCharacterSet characterSetWithCharactersInString:@" ,"]])
        if ([w length])
            [list addObject:w];
    return list;
}

- (void)apply:(id)sender
{
    NSInteger index = _list.selected;
    if (index < 0)
        return;
    SCPreferencesRef prefs = (SCPreferencesRef)CFRetain([self editing]);
    SCNetworkServiceRef service = prefs ? SCNetworkServiceCopy(prefs, (__bridge CFStringRef)_serviceIDs[(NSUInteger)index]) : NULL;
    NSString *method = kMethods[MAX(0, [_method indexOfSelectedItem])];
    BOOL ok = service != NULL;

    if (ok) {
        NSMutableDictionary *config = [NSMutableDictionary dictionaryWithObject:method forKey:@"ConfigMethod"];
        if ([method isEqual:@"Manual"] || [method isEqual:@"INFORM"])
            config[@"Addresses"] = @[ [_address stringValue] ];
        if ([method isEqual:@"Manual"]) {
            config[@"SubnetMasks"] = @[ [_mask stringValue] ];
            if ([[_router stringValue] length])
                config[@"Router"] = [_router stringValue];
        }
        ok = [self setProtocol:kSCNetworkProtocolTypeIPv4 of:service to:config];
        NSMutableDictionary *dns = [NSMutableDictionary dictionary];
        if ([words(_dns) count])
            dns[@"ServerAddresses"] = words(_dns);
        if ([words(_domains) count])
            dns[@"SearchDomains"] = words(_domains);
        if (ok)
            ok = [self setProtocol:kSCNetworkProtocolTypeDNS of:service to:dns];
        ok = ok && SCPreferencesCommitChanges(prefs) && SCPreferencesApplyChanges(prefs);
    }
    os_log_info(OS_LOG_DEFAULT, "network %{public}s %{public}s: %d", [_serviceNames[(NSUInteger)index] UTF8String],
                method.UTF8String, ok);
    [self dropEditing];
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
    /* Nothing left (no DNS servers or domains of its own): its
       configuration removed (NULL), not an empty one kept. */
    ok = SCNetworkProtocolSetConfiguration(p, [config count] ? (__bridge CFDictionaryRef)config : NULL) &&
         SCNetworkProtocolSetEnabled(p, true);
    CFRelease(p);
    return ok;
}

@end
