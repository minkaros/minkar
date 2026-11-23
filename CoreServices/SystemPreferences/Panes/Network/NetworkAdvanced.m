/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network pane's Advanced sheet, as the reference's, for one service:
 *   TCP/IP    Configure IPv4 (the addresses it has now, Renew DHCP Lease,
 *             the DHCP client ID) and Configure IPv6 (Automatically,
 *             Manually, Link-local only, Off; its router and addresses now)
 *   DNS       the DNS servers and the search domains, lists to add to
 *   Proxies   the protocols, each on or off with its server and port (the
 *             automatic configuration's URL), Exclude simple hostnames,
 *             the hosts and domains that bypass them, Passive FTP
 *   Hardware  the MAC address, Configure (Automatically, Manually), Speed,
 *             Duplex and MTU, SystemConfiguration's media options
 * WINS (SMB) and 802.1X (no supplicant yet) are left out. OK writes into
 * the pane's preferences being edited; the pane's Apply commits them, its
 * Revert drops them, as on the reference.
 */

#import "NetworkAdvanced.h"
#import <SystemConfiguration/SystemConfiguration.h>

static const CGFloat kSheetWidth = 600, kSheetHeight = 430;
/* The width a tab's view has inside the tab view. */
static const CGFloat kPageWidth = 520;

static NSTextField *alabel(NSString *text, NSRect frame, BOOL right)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text ?: @""];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:right ? NSRightTextAlignment : NSLeftTextAlignment];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

static NSTextField *aentry(NSRect frame)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setEditable:YES];
    [field setBezeled:YES];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

static NSString *afirst(id array)
{
    return [array isKindOfClass:[NSArray class]] && [array count] ? [[array firstObject] description] : @"";
}

/* An editable list with + and - below it (DNS servers, search domains). */
@interface NetworkEditableList : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSMutableArray<NSString *> *items;
@property (nonatomic, strong) NSTableView *table;
- (void)placeIn:(NSView *)view frame:(NSRect)frame;
@end

@implementation NetworkEditableList

- (void)placeIn:(NSView *)view frame:(NSRect)frame
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(NSMinX(frame), NSMinY(frame) + 24,
                                                                          NSWidth(frame), NSHeight(frame) - 24)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    self.table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"item"];
    [column setWidth:NSWidth(frame) - 20];
    [column setEditable:YES];
    [self.table addTableColumn:column];
    [self.table setHeaderView:nil];
    [self.table setDataSource:self];
    [self.table setDelegate:self];
    [scroll setDocumentView:self.table];
    [view addSubview:scroll];

    NSButton *add = [[NSButton alloc] initWithFrame:NSMakeRect(NSMinX(frame), NSMinY(frame), 26, 22)];
    [add setTitle:@"+"];
    [add setTarget:self];
    [add setAction:@selector(add:)];
    NSButton *remove = [[NSButton alloc] initWithFrame:NSMakeRect(NSMinX(frame) + 25, NSMinY(frame), 26, 22)];
    [remove setTitle:@"−"];
    [remove setTarget:self];
    [remove setAction:@selector(remove:)];
    [view addSubview:add];
    [view addSubview:remove];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return [self.items count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    return self.items[row];
}

- (void)tableView:(NSTableView *)table setObjectValue:(id)value forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NSString *text = [[value description] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if ([text length])
        self.items[row] = text;
    else
        [self.items removeObjectAtIndex:row];
    [table reloadData];
}

- (void)add:(id)sender
{
    [self.items addObject:@""];
    [self.table reloadData];
    NSInteger row = [self.items count] - 1;
    [self.table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    [self.table editColumn:0 row:row withEvent:nil select:YES];
}

- (void)remove:(id)sender
{
    NSInteger row = [self.table selectedRow];
    if (row >= 0 && row < (NSInteger)[self.items count]) {
        [self.items removeObjectAtIndex:row];
        [self.table reloadData];
    }
}

/* The items kept: not empty. */
- (NSArray *)values
{
    [[self.table window] makeFirstResponder:nil];
    NSMutableArray *kept = [NSMutableArray array];
    for (NSString *s in self.items)
        if ([s length])
            [kept addObject:s];
    return kept;
}

@end

/* The proxies' protocols, in the reference's order, with their keys. */
typedef struct {
    NSString *title, *enable, *server, *port;
} ProxyKind;

static const ProxyKind kProxies[] = {
    { @"Auto Proxy Discovery", @"ProxyAutoDiscoveryEnable", nil, nil },
    { @"Automatic Proxy Configuration", @"ProxyAutoConfigEnable", @"ProxyAutoConfigURLString", nil },
    { @"Web Proxy (HTTP)", @"HTTPEnable", @"HTTPProxy", @"HTTPPort" },
    { @"Secure Web Proxy (HTTPS)", @"HTTPSEnable", @"HTTPSProxy", @"HTTPSPort" },
    { @"FTP Proxy", @"FTPEnable", @"FTPProxy", @"FTPPort" },
    { @"SOCKS Proxy", @"SOCKSEnable", @"SOCKSProxy", @"SOCKSPort" },
    { @"Streaming Proxy (RTSP)", @"RTSPEnable", @"RTSPProxy", @"RTSPPort" },
    { @"Gopher Proxy", @"GopherEnable", @"GopherProxy", @"GopherPort" },
};
static const NSUInteger kProxyCount = sizeof(kProxies) / sizeof(kProxies[0]);

static NSString *const kIPv6Methods[] = { @"Automatic", @"Manual", @"LinkLocal", @"Off" };

@interface NetworkAdvanced () <NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation NetworkAdvanced {
    SCPreferencesRef _prefs;
    NSString *_serviceID;
    NSBundle *_bundle;
    NSWindow *_sheet, *_parent;
    void (^_done)(BOOL);

    NSPopUpButton *_v4Method, *_v6Method, *_configure, *_speed, *_duplex, *_mtu;
    NSTextField *_clientID, *_v6Address, *_v6Prefix, *_v6Router, *_proxyServer, *_proxyPort, *_proxyLabel;
    NetworkEditableList *_servers, *_domains;
    NSTableView *_proxyTable;
    NSMutableDictionary *_proxies;
    NSButton *_excludeSimple, *_passiveFTP;
    NSTextView *_bypass;
    NSArray *_subtypes;
}

- (instancetype)initWithPreferences:(SCPreferencesRef)prefs serviceID:(NSString *)serviceID bundle:(NSBundle *)bundle
{
    if ((self = [super init]) == nil)
        return nil;
    _prefs = (SCPreferencesRef)CFRetain(prefs);
    _serviceID = [serviceID copy];
    _bundle = bundle;
    return self;
}

- (void)dealloc
{
    CFRelease(_prefs);
}

- (NSString *)text:(NSString *)english
{
    return [_bundle localizedStringForKey:english value:english table:nil];
}

- (SCNetworkServiceRef)copyService
{
    return SCNetworkServiceCopy(_prefs, (__bridge CFStringRef)_serviceID);
}

- (NSDictionary *)configOf:(CFStringRef)type
{
    SCNetworkServiceRef service = [self copyService];
    SCNetworkProtocolRef p = service ? SCNetworkServiceCopyProtocol(service, type) : NULL;
    NSDictionary *config = p ? [(__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(p) copy] : nil;

    if (p)
        CFRelease(p);
    if (service)
        CFRelease(service);
    return config ?: @{};
}

- (NSDictionary *)state:(NSString *)entity
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("Network pane"), NULL, NULL);
    CFStringRef key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState,
        (__bridge CFStringRef)_serviceID, (__bridge CFStringRef)entity);
    NSDictionary *state = store ? (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(store, key) : nil;

    CFRelease(key);
    if (store)
        CFRelease(store);
    return [state isKindOfClass:[NSDictionary class]] ? state : @{};
}

/* ---- The tabs ---- */

- (NSView *)tcpipView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kPageWidth, 320)];
    NSDictionary *v4 = [self configOf:kSCNetworkProtocolTypeIPv4], *now = [self state:@"IPv4"];
    NSDictionary *v6 = [self configOf:kSCNetworkProtocolTypeIPv6], *now6 = [self state:@"IPv6"];
    CGFloat x = 150, y = 280;

    [view addSubview:alabel([self text:@"Configure IPv4:"], NSMakeRect(0, y, 140, 20), YES)];
    _v4Method = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 240, 26) pullsDown:NO];
    for (NSString *t in @[ @"Using DHCP", @"Using DHCP with manual address", @"Using BootP", @"Manually", @"Off" ])
        [_v4Method addItemWithTitle:[self text:t]];
    NSArray *methods = @[ @"DHCP", @"INFORM", @"BOOTP", @"Manual", @"Off" ];
    NSUInteger m = [methods indexOfObject:v4[@"ConfigMethod"] ?: @"DHCP"];
    [_v4Method selectItemAtIndex:m == NSNotFound ? 0 : m];
    [_v4Method setEnabled:NO];   /* chosen in the pane, shown here */
    [view addSubview:_v4Method];

    y -= 30;
    NSArray *rows = @[ @[ @"IPv4 Address:", afirst(now[@"Addresses"]) ],
                       @[ @"Subnet Mask:", afirst(now[@"SubnetMasks"]) ],
                       @[ @"Router:", [now[@"Router"] description] ?: @"" ] ];
    for (NSArray *r in rows) {
        [view addSubview:alabel([self text:r[0]], NSMakeRect(0, y, 140, 20), YES)];
        [view addSubview:alabel(r[1], NSMakeRect(x, y, 220, 20), NO)];
        y -= 24;
    }
    NSButton *renew = [[NSButton alloc] initWithFrame:NSMakeRect(kPageWidth - 150, y + 70, 150, 26)];
    [renew setTitle:[self text:@"Renew DHCP Lease"]];
    [renew setBezelStyle:NSRoundedBezelStyle];
    [renew setTarget:self];
    [renew setAction:@selector(renewLease:)];
    [view addSubview:renew];
    [view addSubview:alabel([self text:@"DHCP Client ID:"], NSMakeRect(0, y, 140, 20), YES)];
    _clientID = aentry(NSMakeRect(x, y - 2, 220, 24));
    [_clientID setStringValue:v4[@"DHCPClientID"] ?: @""];
    [view addSubview:_clientID];
    [view addSubview:alabel([self text:@"(If required)"], NSMakeRect(x + 228, y, 120, 20), NO)];

    y -= 46;
    [view addSubview:alabel([self text:@"Configure IPv6:"], NSMakeRect(0, y, 140, 20), YES)];
    _v6Method = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 240, 26) pullsDown:NO];
    for (NSString *t in @[ @"Automatically", @"Manually", @"Link-local only", @"Off" ])
        [_v6Method addItemWithTitle:[self text:t]];
    NSString *method6 = v6[@"ConfigMethod"] ?: ([v6 count] ? @"Automatic" : @"Off");
    for (NSUInteger i = 0; i < 4; i++)
        if ([kIPv6Methods[i] isEqual:method6])
            [_v6Method selectItemAtIndex:i];
    [_v6Method setTarget:self];
    [_v6Method setAction:@selector(chooseIPv6:)];
    [view addSubview:_v6Method];

    y -= 30;
    [view addSubview:alabel([self text:@"Router:"], NSMakeRect(0, y, 140, 20), YES)];
    _v6Router = aentry(NSMakeRect(x, y - 2, 260, 24));
    [_v6Router setStringValue:[method6 isEqual:@"Manual"] ? (v6[@"Router"] ?: @"") : ([now6[@"Router"] description] ?: @"")];
    [view addSubview:_v6Router];
    y -= 30;
    [view addSubview:alabel([self text:@"IPv6 Address:"], NSMakeRect(0, y, 140, 20), YES)];
    _v6Address = aentry(NSMakeRect(x, y - 2, 260, 24));
    [_v6Address setStringValue:afirst([method6 isEqual:@"Manual"] ? v6[@"Addresses"] : now6[@"Addresses"])];
    [view addSubview:_v6Address];
    y -= 30;
    [view addSubview:alabel([self text:@"Prefix Length:"], NSMakeRect(0, y, 140, 20), YES)];
    _v6Prefix = aentry(NSMakeRect(x, y - 2, 60, 24));
    [_v6Prefix setStringValue:afirst([method6 isEqual:@"Manual"] ? v6[@"PrefixLength"] : now6[@"PrefixLength"])];
    [view addSubview:_v6Prefix];
    [self chooseIPv6:nil];
    return view;
}

- (void)chooseIPv6:(id)sender
{
    BOOL manual = [_v6Method indexOfSelectedItem] == 1;
    for (NSTextField *f in @[ _v6Router, _v6Address, _v6Prefix ]) {
        [f setEditable:manual];
        [f setTextColor:manual ? [NSColor controlTextColor] : [NSColor secondaryLabelColor]];
    }
}

/* The interface asks DHCP again, as Renew DHCP Lease does. */
- (void)renewLease:(id)sender
{
    SCNetworkServiceRef service = [self copyService];
    SCNetworkInterfaceRef interface = service ? SCNetworkServiceGetInterface(service) : NULL;

    if (interface == NULL || !SCNetworkInterfaceForceConfigurationRefresh(interface))
        NSBeep();
    if (service)
        CFRelease(service);
}

- (NSView *)dnsView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kPageWidth, 320)];
    NSDictionary *dns = [self configOf:kSCNetworkProtocolTypeDNS];
    CGFloat half = (kPageWidth - 20) / 2;

    [view addSubview:alabel([self text:@"DNS Servers:"], NSMakeRect(0, 290, half, 20), NO)];
    _servers = [NetworkEditableList new];
    _servers.items = [NSMutableArray arrayWithArray:[dns[@"ServerAddresses"] isKindOfClass:[NSArray class]] ?
                                                    dns[@"ServerAddresses"] : @[]];
    [_servers placeIn:view frame:NSMakeRect(0, 30, half, 256)];
    [view addSubview:alabel([self text:@"IPv4 or IPv6 addresses"], NSMakeRect(60, 30, half - 60, 20), NO)];

    [view addSubview:alabel([self text:@"Search Domains:"], NSMakeRect(half + 20, 290, half, 20), NO)];
    _domains = [NetworkEditableList new];
    _domains.items = [NSMutableArray arrayWithArray:[dns[@"SearchDomains"] isKindOfClass:[NSArray class]] ?
                                                    dns[@"SearchDomains"] : @[]];
    [_domains placeIn:view frame:NSMakeRect(half + 20, 30, half, 256)];
    return view;
}

- (NSView *)proxiesView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kPageWidth, 320)];
    _proxies = [[self configOf:kSCNetworkProtocolTypeProxies] mutableCopy];

    [view addSubview:alabel([self text:@"Select a protocol to configure:"], NSMakeRect(0, 296, 260, 20), NO)];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 110, 250, 182)];
    [scroll setBorderType:NSBezelBorder];
    _proxyTable = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *on = [[NSTableColumn alloc] initWithIdentifier:@"on"];
    [on setWidth:20];
    NSButtonCell *check = [[NSButtonCell alloc] init];
    [check setButtonType:NSSwitchButton];
    [check setTitle:@""];
    [on setDataCell:check];
    NSTableColumn *name = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [name setWidth:210];
    [name setEditable:NO];
    [_proxyTable addTableColumn:on];
    [_proxyTable addTableColumn:name];
    [_proxyTable setHeaderView:nil];
    [_proxyTable setDataSource:self];
    [_proxyTable setDelegate:self];
    [scroll setDocumentView:_proxyTable];
    [view addSubview:scroll];

    _proxyLabel = alabel(@"", NSMakeRect(265, 270, kPageWidth - 265, 20), NO);
    [view addSubview:_proxyLabel];
    _proxyServer = aentry(NSMakeRect(265, 244, kPageWidth - 265 - 70, 24));
    [_proxyServer setTarget:self];
    [_proxyServer setAction:@selector(proxyEdited:)];
    [view addSubview:_proxyServer];
    _proxyPort = aentry(NSMakeRect(kPageWidth - 60, 244, 60, 24));
    [_proxyPort setTarget:self];
    [_proxyPort setAction:@selector(proxyEdited:)];
    [view addSubview:_proxyPort];

    _excludeSimple = [[NSButton alloc] initWithFrame:NSMakeRect(0, 82, 300, 22)];
    [_excludeSimple setButtonType:NSSwitchButton];
    [_excludeSimple setTitle:[self text:@"Exclude simple hostnames"]];
    [_excludeSimple setState:[_proxies[@"ExcludeSimpleHostnames"] boolValue] ? NSOnState : NSOffState];
    [view addSubview:_excludeSimple];
    [view addSubview:alabel([self text:@"Bypass proxy settings for these Hosts & Domains:"],
                            NSMakeRect(0, 60, 400, 20), NO)];
    NSScrollView *bypassScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 28, kPageWidth, 32)];
    [bypassScroll setBorderType:NSBezelBorder];
    _bypass = [[NSTextView alloc] initWithFrame:[[bypassScroll contentView] bounds]];
    NSArray *exceptions = _proxies[@"ExceptionsList"];
    [_bypass setString:[exceptions isKindOfClass:[NSArray class]] ? [exceptions componentsJoinedByString:@", "]
                                                                  : @"*.local, 169.254/16"];
    [bypassScroll setDocumentView:_bypass];
    [view addSubview:bypassScroll];
    _passiveFTP = [[NSButton alloc] initWithFrame:NSMakeRect(0, 2, 300, 22)];
    [_passiveFTP setButtonType:NSSwitchButton];
    [_passiveFTP setTitle:[self text:@"Use Passive FTP Mode (PASV)"]];
    [_passiveFTP setState:_proxies[@"FTPPassive"] == nil || [_proxies[@"FTPPassive"] boolValue] ? NSOnState : NSOffState];
    [view addSubview:_passiveFTP];

    [_proxyTable selectRowIndexes:[NSIndexSet indexSetWithIndex:2] byExtendingSelection:NO];
    [self showProxy];
    return view;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return kProxyCount;
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if ([[column identifier] isEqual:@"on"])
        return @([_proxies[kProxies[row].enable] boolValue]);
    return [self text:kProxies[row].title];
}

- (void)tableView:(NSTableView *)table setObjectValue:(id)value forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if ([[column identifier] isEqual:@"on"])
        _proxies[kProxies[row].enable] = @([value boolValue] ? 1 : 0);
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [self showProxy];
}

/* The chosen protocol's server and port (the automatic configuration's
   URL, or nothing for discovery). */
- (void)showProxy
{
    NSInteger row = [_proxyTable selectedRow];
    if (row < 0)
        return;
    const ProxyKind *k = &kProxies[row];
    [_proxyLabel setStringValue:k->server == nil ? @"" : k->port == nil ? [self text:@"URL:"]
                              : [NSString stringWithFormat:[self text:@"%@ Server:"], [self text:k->title]]];
    [_proxyServer setHidden:k->server == nil];
    [_proxyPort setHidden:k->port == nil];
    [_proxyServer setFrame:NSMakeRect(265, 244, kPageWidth - 265 - (k->port == nil ? 0 : 70), 24)];
    [_proxyServer setStringValue:k->server ? (_proxies[k->server] ?: @"") : @""];
    [_proxyPort setStringValue:k->port && _proxies[k->port] ? [_proxies[k->port] description] : @""];
}

- (void)proxyEdited:(id)sender
{
    NSInteger row = [_proxyTable selectedRow];
    if (row < 0)
        return;
    const ProxyKind *k = &kProxies[row];
    if (k->server)
        _proxies[k->server] = [_proxyServer stringValue];
    if (k->port && [[_proxyPort stringValue] length])
        _proxies[k->port] = @([_proxyPort intValue]);
}

- (NSView *)hardwareView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kPageWidth, 320)];
    SCNetworkServiceRef service = [self copyService];
    SCNetworkInterfaceRef interface = service ? SCNetworkServiceGetInterface(service) : NULL;
    CFDictionaryRef current = NULL, active = NULL;
    CFArrayRef available = NULL;
    int mtu = 0, mtuMin = 0, mtuMax = 0;
    CGFloat x = 150, y = 280;

    [view addSubview:alabel([self text:@"MAC Address:"], NSMakeRect(0, y, 140, 20), YES)];
    NSString *mac = interface ? (__bridge NSString *)SCNetworkInterfaceGetHardwareAddressString(interface) : nil;
    [view addSubview:alabel(mac ?: @"", NSMakeRect(x, y, 240, 20), NO)];

    if (interface)
        SCNetworkInterfaceCopyMediaOptions(interface, &current, &active, &available, false);
    y -= 34;
    [view addSubview:alabel([self text:@"Configure:"], NSMakeRect(0, y, 140, 20), YES)];
    _configure = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 200, 26) pullsDown:NO];
    [_configure addItemsWithTitles:@[ [self text:@"Automatically"], [self text:@"Manually"] ]];
    NSString *subtype = current ? (__bridge NSString *)CFDictionaryGetValue(current, kSCPropNetEthernetMediaSubType) : nil;
    [_configure selectItemAtIndex:subtype == nil || [subtype isEqual:@"autoselect"] ? 0 : 1];
    [_configure setTarget:self];
    [_configure setAction:@selector(chooseConfigure:)];
    [view addSubview:_configure];

    y -= 30;
    [view addSubview:alabel([self text:@"Speed:"], NSMakeRect(0, y, 140, 20), YES)];
    _speed = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 200, 26) pullsDown:NO];
    _subtypes = available ? (__bridge_transfer NSArray *)SCNetworkInterfaceCopyMediaSubTypes(available) : @[];
    NSMutableArray *speeds = [NSMutableArray array];
    for (NSString *s in _subtypes)
        if (![s isEqual:@"autoselect"] && ![s isEqual:@"none"])
            [speeds addObject:s];
    _subtypes = speeds;
    if ([speeds count])
        [_speed addItemsWithTitles:speeds];
    else
        [_speed addItemWithTitle:@"autoselect"];
    NSString *now = active ? (__bridge NSString *)CFDictionaryGetValue(active, kSCPropNetEthernetMediaSubType) : nil;
    /* Manually: the subtype set; automatically: the one in use now. */
    NSString *shown = subtype != nil && ![subtype isEqual:@"autoselect"] ? subtype : now;
    if ([speeds containsObject:shown ?: @""])
        [_speed selectItemWithTitle:shown];
    [_speed setTarget:self];
    [_speed setAction:@selector(chooseSpeed:)];
    [view addSubview:_speed];

    y -= 30;
    [view addSubview:alabel([self text:@"Duplex:"], NSMakeRect(0, y, 140, 20), YES)];
    _duplex = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 200, 26) pullsDown:NO];
    [view addSubview:_duplex];
    [self chooseSpeed:nil];
    NSArray *options = current ? (__bridge NSArray *)CFDictionaryGetValue(current, kSCPropNetEthernetMediaOptions)
                               : active ? (__bridge NSArray *)CFDictionaryGetValue(active, kSCPropNetEthernetMediaOptions) : nil;
    for (NSString *o in options)
        if ([_duplex itemWithTitle:o])
            [_duplex selectItemWithTitle:o];

    y -= 30;
    [view addSubview:alabel([self text:@"MTU:"], NSMakeRect(0, y, 140, 20), YES)];
    _mtu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y - 4, 200, 26) pullsDown:NO];
    [_mtu addItemWithTitle:[self text:@"Standard  (1500)"]];
    if (interface && SCNetworkInterfaceCopyMTU(interface, &mtu, &mtuMin, &mtuMax) && mtuMax >= 9000)
        [_mtu addItemWithTitle:[self text:@"Jumbo  (9000)"]];
    [_mtu selectItemAtIndex:mtu >= 9000 && [_mtu numberOfItems] > 1 ? 1 : 0];
    [view addSubview:_mtu];
    [self chooseConfigure:nil];

    if (current)
        CFRelease(current);
    if (active)
        CFRelease(active);
    if (available)
        CFRelease(available);
    if (service)
        CFRelease(service);
    return view;
}

- (void)chooseConfigure:(id)sender
{
    BOOL manual = [_configure indexOfSelectedItem] == 1;
    for (NSPopUpButton *p in @[ _speed, _duplex, _mtu ])
        [p setEnabled:manual];
}

/* The duplex choices of the chosen speed. */
- (void)chooseSpeed:(id)sender
{
    SCNetworkServiceRef service = [self copyService];
    SCNetworkInterfaceRef interface = service ? SCNetworkServiceGetInterface(service) : NULL;
    CFArrayRef available = NULL;

    [_duplex removeAllItems];
    if (interface && SCNetworkInterfaceCopyMediaOptions(interface, NULL, NULL, &available, false) && available) {
        NSArray *choices = (__bridge_transfer NSArray *)SCNetworkInterfaceCopyMediaSubTypeOptions(available,
            (__bridge CFStringRef)[_speed titleOfSelectedItem]);
        for (NSArray *set in choices)
            [_duplex addItemWithTitle:[set count] ? [set componentsJoinedByString:@", "] : [self text:@"none"]];
        CFRelease(available);
    }
    if ([_duplex numberOfItems] == 0)
        [_duplex addItemWithTitle:@"full-duplex"];
    if (service)
        CFRelease(service);
}

/* ---- The sheet ---- */

- (void)beginSheetForWindow:(NSWindow *)window completion:(void (^)(BOOL changed))done
{
    _done = [done copy];
    _parent = window;
    _sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, kSheetWidth, kSheetHeight)
                                         styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    NSView *content = [_sheet contentView];
    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(20, 50, kSheetWidth - 40, kSheetHeight - 60)];
    NSArray *pages = @[ @[ @"TCP/IP", [self tcpipView] ], @[ @"DNS", [self dnsView] ],
                        @[ @"Proxies", [self proxiesView] ], @[ @"Hardware", [self hardwareView] ] ];
    for (NSArray *page in pages) {
        NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:page[0]];
        [item setLabel:[self text:page[0]]];
        [item setView:page[1]];
        [tabs addTabViewItem:item];
    }
    [content addSubview:tabs];

    NSButton *cancel = [[NSButton alloc] initWithFrame:NSMakeRect(kSheetWidth - 220, 12, 96, 28)];
    [cancel setTitle:[self text:@"Cancel"]];
    [cancel setBezelStyle:NSRoundedBezelStyle];
    [cancel setKeyEquivalent:@"\033"];
    [cancel setTarget:self];
    [cancel setAction:@selector(cancel:)];
    NSButton *ok = [[NSButton alloc] initWithFrame:NSMakeRect(kSheetWidth - 116, 12, 96, 28)];
    [ok setTitle:[self text:@"OK"]];
    [ok setBezelStyle:NSRoundedBezelStyle];
    [ok setKeyEquivalent:@"\r"];
    [ok setTarget:self];
    [ok setAction:@selector(ok:)];
    [content addSubview:cancel];
    [content addSubview:ok];
    [window beginSheet:_sheet completionHandler:nil];
}

- (void)finish:(BOOL)changed
{
    /* Ended on the window it hangs from, so the next sheet hangs there too. */
    [_parent endSheet:_sheet];
    [_sheet orderOut:nil];
    if (_done)
        _done(changed);
    _done = nil;
}

- (void)cancel:(id)sender
{
    [self finish:NO];
}

static BOOL setProtocol(SCNetworkServiceRef service, CFStringRef type, NSDictionary *config, BOOL enabled)
{
    SCNetworkProtocolRef p = SCNetworkServiceCopyProtocol(service, type);
    BOOL ok;

    if (p == NULL && (!SCNetworkServiceAddProtocolType(service, type) ||
                      (p = SCNetworkServiceCopyProtocol(service, type)) == NULL))
        return NO;
    ok = SCNetworkProtocolSetConfiguration(p, [config count] ? (__bridge CFDictionaryRef)config : NULL) &&
         SCNetworkProtocolSetEnabled(p, enabled);
    CFRelease(p);
    return ok;
}

/* OK: what was chosen written into the preferences being edited (the
   pane's Apply commits it). */
- (void)ok:(id)sender
{
    [_sheet makeFirstResponder:nil];
    [self proxyEdited:nil];
    SCNetworkServiceRef service = [self copyService];
    BOOL ok = service != NULL;

    if (ok) {
        NSMutableDictionary *v4 = [[self configOf:kSCNetworkProtocolTypeIPv4] mutableCopy];
        NSString *clientID = [[_clientID stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if ([clientID length])
            v4[@"DHCPClientID"] = clientID;
        else
            [v4 removeObjectForKey:@"DHCPClientID"];
        ok = setProtocol(service, kSCNetworkProtocolTypeIPv4, v4, YES);

        NSString *method6 = kIPv6Methods[MAX(0, [_v6Method indexOfSelectedItem])];
        NSMutableDictionary *v6 = [NSMutableDictionary dictionary];
        if (![method6 isEqual:@"Off"])
            v6[@"ConfigMethod"] = method6;
        if ([method6 isEqual:@"Manual"]) {
            v6[@"Addresses"] = @[ [_v6Address stringValue] ];
            v6[@"PrefixLength"] = @[ @([_v6Prefix intValue] ?: 64) ];
            if ([[_v6Router stringValue] length])
                v6[@"Router"] = [_v6Router stringValue];
        }
        ok = ok && setProtocol(service, kSCNetworkProtocolTypeIPv6, v6, ![method6 isEqual:@"Off"]);

        NSMutableDictionary *dns = [NSMutableDictionary dictionary];
        if ([[_servers values] count])
            dns[@"ServerAddresses"] = [_servers values];
        if ([[_domains values] count])
            dns[@"SearchDomains"] = [_domains values];
        ok = ok && setProtocol(service, kSCNetworkProtocolTypeDNS, dns, YES);

        _proxies[@"ExcludeSimpleHostnames"] = @([_excludeSimple state] == NSOnState ? 1 : 0);
        _proxies[@"FTPPassive"] = @([_passiveFTP state] == NSOnState ? 1 : 0);
        NSMutableArray *bypass = [NSMutableArray array];
        for (NSString *w in [[_bypass string] componentsSeparatedByCharactersInSet:
                                [NSCharacterSet characterSetWithCharactersInString:@", \n"]])
            if ([w length])
                [bypass addObject:w];
        _proxies[@"ExceptionsList"] = bypass;
        ok = ok && setProtocol(service, kSCNetworkProtocolTypeProxies, _proxies, YES);

        SCNetworkInterfaceRef interface = SCNetworkServiceGetInterface(service);
        if (interface != NULL) {
            if ([_configure indexOfSelectedItem] == 0)
                SCNetworkInterfaceSetMediaOptions(interface, CFSTR("autoselect"), NULL);
            else {
                NSString *duplex = [_duplex titleOfSelectedItem];
                NSArray *options = [duplex isEqual:[self text:@"none"]] ? @[] : [duplex componentsSeparatedByString:@", "];
                SCNetworkInterfaceSetMediaOptions(interface, (__bridge CFStringRef)[_speed titleOfSelectedItem],
                                                  (__bridge CFArrayRef)options);
                SCNetworkInterfaceSetMTU(interface, [_mtu indexOfSelectedItem] == 1 ? 9000 : 1500);
            }
        }
    }
    if (service)
        CFRelease(service);
    if (!ok)
        NSBeep();
    [self finish:ok];
}

@end
