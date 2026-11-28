/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Printers pane (the maintainer's choice, 23 Nov 2026), laid out as
 * the reference's: the printers CUPS has on the left, with + and -; the
 * chosen one's name, location, kind and status on the right, or "No
 * printers are available" with none; the default printer (Last Printer
 * Used, or one) and the default paper size below. + shows the printers
 * CUPS finds (its backends: the network's by DNS-SD, USB) and adds the
 * chosen one, or the one at an address typed: an IPP Everywhere queue,
 * or for a USB printer one of CUPS's generic PDF models (modelFor);
 * - deletes one, after a question; both are an administrator's, behind
 * the pane's lock (system.preferences.printing), and lpadmin runs as root
 * with its authorization. The defaults are each user's, CUPS's own: org.cups.PrintingPrefs's UseLastPrinter and
 * DefaultPaperID (read by libcups), the default printer by lpoptions -d.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <cups/cups.h>
#include <cups/adminutil.h>
#include <ctype.h>
#include <notify.h>
#include <os/log.h>
#import "../PaneLock.h"

@interface PrintersPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate, NSTextFieldDelegate,
                                            SFAuthorizationViewDelegate>
@end

static const CGFloat kWidth = 668, kHeight = 400;
static CFStringRef const kPrintingPrefs = CFSTR("org.cups.PrintingPrefs");

/* The default paper sizes offered: their names, and their PWG names. */
static NSArray<NSArray<NSString *> *> *paperSizes(void)
{
    return @[ @[ @"A4", @"iso_a4_210x297mm" ], @[ @"US Letter", @"na_letter_8.5x11in" ],
              @[ @"US Legal", @"na_legal_8.5x14in" ], @[ @"A3", @"iso_a3_297x420mm" ],
              @[ @"A5", @"iso_a5_148x210mm" ], @[ @"B5", @"iso_b5_176x250mm" ] ];
}

static NSTextField *label(NSString *text, NSRect frame, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    return field;
}

/* Runs a CUPS command and waits for it; its status. */
static int runTool(NSString *path, NSArray<NSString *> *arguments)
{
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:path];
    [task setArguments:arguments];
    @try {
        [task launch];
    } @catch (NSException *e) {
        return -1;
    }
    [task waitUntilExit];
    return [task terminationStatus];
}

/* A queue's name from a printer's: letters, digits and underscores. */
static NSString *queueName(NSString *name)
{
    NSMutableString *queue = [NSMutableString string];
    for (NSUInteger i = 0; i < [name length] && [queue length] < 127; i++) {
        unichar c = [name characterAtIndex:i];
        [queue appendString:(c < 128 && (isalnum(c) || c == '_' || c == '-')) ? [NSString stringWithCharacters:&c length:1] : @"_"];
    }
    return [queue length] ? queue : @"Printer";
}

@implementation PrintersPane {
    NSTableView *_list;
    NSButton *_remove, *_plus;
    SFAuthorizationView *_lock;
    NSBox *_detail;
    NSTextField *_none, *_name, *_location, *_kind, *_status;
    NSPopUpButton *_defaultPrinter, *_paper;
    NSArray<NSDictionary *> *_printers;     /* name, location, kind, status */
    /* The Add sheet. */
    NSWindow *_sheet;
    NSTableView *_found;
    NSMutableArray<NSDictionary *> *_devices;  /* uri, info, make */
    NSTextField *_addName, *_addLocation, *_addAddress, *_searching;
    NSButton *_add;
    int _notifyToken;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (void)dealloc
{
    if (_notifyToken != 0)
        notify_cancel(_notifyToken);
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 94, 198, 286)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _list = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [column setWidth:194];
    [column setEditable:NO];
    [_list addTableColumn:column];
    [_list setHeaderView:nil];
    [_list setRowHeight:22];
    [_list setDataSource:self];
    [_list setDelegate:self];
    [scroll setDocumentView:_list];
    [view addSubview:scroll];
    _plus = [[NSButton alloc] initWithFrame:NSMakeRect(20, 72, 26, 22)];
    [_plus setTitle:@"+"];
    [_plus setBezelStyle:NSSmallSquareBezelStyle];
    [_plus setTarget:self];
    [_plus setAction:@selector(addPrinter:)];
    [view addSubview:_plus];
    _remove = [[NSButton alloc] initWithFrame:NSMakeRect(45, 72, 26, 22)];
    [_remove setTitle:@"−"];
    [_remove setBezelStyle:NSSmallSquareBezelStyle];
    [_remove setTarget:self];
    [_remove setAction:@selector(removePrinter:)];
    [view addSubview:_remove];

    _detail = [[NSBox alloc] initWithFrame:NSMakeRect(228, 70, 420, 314)];
    [_detail setTitlePosition:NSNoTitle];
    NSView *box = [_detail contentView];
    _none = label([self text:@"No printers are available.\nClick Add (+) to set up a printer."],
                  NSMakeRect(20, 130, 370, 40), NSCenterTextAlignment);
    [box addSubview:_none];
    _name = label(@"", NSMakeRect(20, 250, 370, 24), NSLeftTextAlignment);
    [_name setFont:[NSFont boldSystemFontOfSize:15]];
    [box addSubview:_name];
    NSArray *rows = @[ @"Location:", @"Kind:", @"Status:" ];
    NSMutableArray *values = [NSMutableArray array];
    for (NSUInteger i = 0; i < [rows count]; i++) {
        NSTextField *title = label([self text:rows[i]], NSMakeRect(10, 210 - i * 26, 90, 20), NSRightTextAlignment);
        [title setTag:100 + (NSInteger)i];
        [box addSubview:title];
        NSTextField *value = label(@"", NSMakeRect(106, 210 - i * 26, 284, 20), NSLeftTextAlignment);
        [box addSubview:value];
        [values addObject:value];
    }
    _location = values[0];
    _kind = values[1];
    _status = values[2];
    [view addSubview:_detail];

    [view addSubview:label([self text:@"Default printer:"], NSMakeRect(160, 40, 200, 20), NSRightTextAlignment)];
    _defaultPrinter = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(366, 36, 230, 26) pullsDown:NO];
    [_defaultPrinter setTarget:self];
    [_defaultPrinter setAction:@selector(chooseDefaultPrinter:)];
    [view addSubview:_defaultPrinter];
    [view addSubview:label([self text:@"Default paper size:"], NSMakeRect(160, 10, 200, 20), NSRightTextAlignment)];
    _paper = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(366, 6, 230, 26) pullsDown:NO];
    for (NSArray *size in paperSizes())
        [_paper addItemWithTitle:[self text:size[0]]];
    [_paper setTarget:self];
    [_paper setAction:@selector(choosePaper:)];
    [view addSubview:_paper];

    [self reload];
    /* Changed elsewhere (lpadmin, another app): shown again. */
    __weak PrintersPane *weakSelf = self;
    notify_register_dispatch("com.apple.printerListChange", &_notifyToken, dispatch_get_main_queue(), ^(int token) {
        [weakSelf reload];
    });
    /* Adding and deleting printers are an administrator's: the lock. */
    NSView *pane = paneWithLock(view, @"system.preferences.printing", self, &_lock);
    [self showChosen];
    [self setMainView:pane];
    return pane;
}

- (void)authorizationViewDidAuthorize:(SFAuthorizationView *)view
{
    [self showChosen];
}

- (void)authorizationViewDidDeauthorize:(SFAuthorizationView *)view
{
    [self showChosen];
}

/* ---- The printers ---------------------------------------------------- */

static NSString *option(cups_dest_t *dest, const char *name)
{
    const char *value = cupsGetOption(name, dest->num_options, dest->options);
    return value != NULL ? [NSString stringWithUTF8String:value] : @"";
}

- (NSString *)statusOf:(cups_dest_t *)dest
{
    int state = [option(dest, "printer-state") intValue];
    NSString *reasons = option(dest, "printer-state-reasons");

    if (state == IPP_PSTATE_STOPPED)
        return [self text:@"Paused"];
    if (state == IPP_PSTATE_PROCESSING)
        return [self text:@"Printing"];
    /* What the user can act on, in words; CUPS's own notes (cups-*, such as
       cups-ipp-conformance-failure-report) and mere reports are left out. */
    NSDictionary *words = @{ @"media-empty": @"Out of paper", @"media-needed": @"Out of paper",
                             @"media-jam": @"Paper jam", @"toner-low": @"Toner low",
                             @"toner-empty": @"Out of toner", @"marker-supply-low": @"Toner low",
                             @"marker-supply-empty": @"Out of toner", @"door-open": @"Door open",
                             @"cover-open": @"Door open", @"offline": @"Offline",
                             @"connecting-to-device": @"Connecting" };
    NSMutableArray *shown = [NSMutableArray array];
    for (NSString *reason in [reasons componentsSeparatedByString:@","]) {
        if ([reason isEqualToString:@"none"] || [reason hasPrefix:@"cups-"] || [reason hasSuffix:@"-report"])
            continue;
        NSString *keyword = reason;
        for (NSString *suffix in @[ @"-error", @"-warning" ])
            if ([keyword hasSuffix:suffix])
                keyword = [keyword substringToIndex:[keyword length] - [suffix length]];
        NSString *word = words[keyword];
        if (word != nil && ![shown containsObject:[self text:word]])
            [shown addObject:[self text:word]];
    }
    if ([shown count])
        return [NSString stringWithFormat:@"%@ (%@)", [self text:@"Idle"], [shown componentsJoinedByString:@", "]];
    return [self text:@"Idle"];
}

- (void)reload
{
    cups_dest_t *dests = NULL;
    int count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);
    NSMutableArray *printers = [NSMutableArray array];

    for (int i = 0; i < count; i++) {
        if (dests[i].instance != NULL)
            continue;
        NSString *info = option(&dests[i], "printer-info");
        [printers addObject:@{ @"queue": [NSString stringWithUTF8String:dests[i].name],
                               @"name": [info length] ? info : [NSString stringWithUTF8String:dests[i].name],
                               @"location": option(&dests[i], "printer-location"),
                               @"kind": option(&dests[i], "printer-make-and-model"),
                               @"status": [self statusOf:&dests[i]] }];
    }
    cupsFreeDests(count, dests);
    _printers = printers;
    [_list reloadData];
    if ([_list selectedRow] < 0 && [_printers count] > 0)
        [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [self showChosen];

    /* The default printer: Last Printer Used, or the one lpoptions has. */
    [_defaultPrinter removeAllItems];
    [_defaultPrinter addItemWithTitle:[self text:@"Last Printer Used"]];
    if ([_printers count] > 0)
        [[_defaultPrinter menu] addItem:[NSMenuItem separatorItem]];
    for (NSDictionary *p in _printers) {
        [_defaultPrinter addItemWithTitle:p[@"name"]];
        [[_defaultPrinter lastItem] setRepresentedObject:p[@"queue"]];
    }
    CFPreferencesAppSynchronize(kPrintingPrefs);
    Boolean valid = false;
    Boolean useLast = CFPreferencesGetAppBooleanValue(CFSTR("UseLastPrinter"), kPrintingPrefs, &valid);
    const char *def = cupsGetDefault2(CUPS_HTTP_DEFAULT);
    [_defaultPrinter selectItemAtIndex:0];
    if (valid && !useLast && def != NULL) {
        NSInteger index = [_defaultPrinter indexOfItemWithRepresentedObject:[NSString stringWithUTF8String:def]];
        if (index >= 0)
            [_defaultPrinter selectItemAtIndex:index];
    }

    NSString *paper = CFBridgingRelease(CFPreferencesCopyAppValue(CFSTR("DefaultPaperID"), kPrintingPrefs));
    [_paper selectItemAtIndex:0];
    for (NSUInteger i = 0; i < [paperSizes() count]; i++)
        if ([paperSizes()[i][1] isEqual:paper])
            [_paper selectItemAtIndex:(NSInteger)i];
}

- (void)showChosen
{
    NSInteger row = [_list selectedRow];
    BOOL any = row >= 0 && (NSUInteger)row < [_printers count];

    [_none setHidden:any];
    for (NSView *v in [[_detail contentView] subviews])
        if (v != _none)
            [v setHidden:!any];
    [_remove setEnabled:any && paneUnlocked(_lock)];
    [_plus setEnabled:paneUnlocked(_lock)];
    if (!any)
        return;
    NSDictionary *p = _printers[(NSUInteger)row];
    [_name setStringValue:p[@"name"]];
    [_location setStringValue:p[@"location"]];
    [_kind setStringValue:p[@"kind"]];
    [_status setStringValue:p[@"status"]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return table == _found ? (NSInteger)[_devices count] : (NSInteger)[_printers count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if (table == _found) {
        NSDictionary *d = _devices[(NSUInteger)row];
        return [[column identifier] isEqual:@"kind"] ? d[@"kind"] : d[@"info"];
    }
    return _printers[(NSUInteger)row][@"name"];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    if ([note object] == _list) {
        [self showChosen];
        return;
    }
    NSInteger row = [_found selectedRow];
    [_add setEnabled:row >= 0];
    if (row >= 0)
        [_addName setStringValue:_devices[(NSUInteger)row][@"info"]];
}

/* ---- The defaults ---------------------------------------------------- */

- (void)chooseDefaultPrinter:(NSPopUpButton *)sender
{
    NSString *queue = [[sender selectedItem] representedObject];

    CFPreferencesSetAppValue(CFSTR("UseLastPrinter"), queue == nil ? kCFBooleanTrue : kCFBooleanFalse, kPrintingPrefs);
    CFPreferencesAppSynchronize(kPrintingPrefs);
    if (queue != nil)
        runTool(@"/usr/bin/lpoptions", @[ @"-d", queue ]);
    notify_post("com.apple.printerPrefsChange");
}

- (void)choosePaper:(NSPopUpButton *)sender
{
    NSString *pwg = paperSizes()[(NSUInteger)[sender indexOfSelectedItem]][1];

    CFPreferencesSetAppValue(CFSTR("DefaultPaperID"), (__bridge CFStringRef)pwg, kPrintingPrefs);
    CFPreferencesAppSynchronize(kPrintingPrefs);
    notify_post("com.apple.printerPrefsChange");
}

/* ---- Remove ---------------------------------------------------------- */

- (void)removePrinter:(id)sender
{
    NSInteger row = [_list selectedRow];
    if (row < 0 || (NSUInteger)row >= [_printers count])
        return;
    NSDictionary *p = _printers[(NSUInteger)row];
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:[self text:@"Are you sure you want to delete the printer “%@”?"], p[@"name"]]];
    [alert addButtonWithTitle:[self text:@"Delete Printer"]];
    [alert addButtonWithTitle:[self text:@"Cancel"]];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;
    int status = paneRunTool(_lock, @"/usr/sbin/lpadmin", @[ @"-x", p[@"queue"] ]);
    os_log_info(OS_LOG_DEFAULT, "printers: deleted %{public}s: %d", [p[@"queue"] UTF8String], status);
    [_list deselectAll:nil];
    [self reload];
}

/* ---- Add ------------------------------------------------------------- */

static void foundDevice(const char *deviceClass, const char *deviceID, const char *deviceInfo,
                        const char *deviceMakeAndModel, const char *deviceURI, const char *deviceLocation,
                        void *context)
{
    /* Network and USB printers, not the backends' bare schemes. */
    if (deviceURI == NULL || strchr(deviceURI, ':') == NULL || strstr(deviceURI, "://") == NULL)
        return;
    NSDictionary *device = @{ @"uri": [NSString stringWithUTF8String:deviceURI],
                              @"id": [NSString stringWithUTF8String:deviceID ?: ""],
                              @"info": [NSString stringWithUTF8String:deviceInfo ?: deviceURI],
                              @"kind": [NSString stringWithUTF8String:deviceMakeAndModel ?: ""],
                              @"location": [NSString stringWithUTF8String:deviceLocation ?: ""] };
    NSMutableArray *found = (__bridge NSMutableArray *)context;
    @synchronized (found) {
        [found addObject:device];
    }
}

- (void)addPrinter:(id)sender
{
    _devices = [NSMutableArray array];
    _sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 410)
        styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
    NSView *content = [_sheet contentView];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 180, 480, 210)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _found = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    for (NSArray *c in @[ @[ @"name", @"Name", @300 ], @[ @"kind", @"Kind", @172 ] ]) {
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:c[0]];
        [[column headerCell] setStringValue:[self text:c[1]]];
        [column setWidth:[c[2] doubleValue]];
        [column setEditable:NO];
        [_found addTableColumn:column];
    }
    [_found setDataSource:self];
    [_found setDelegate:self];
    [scroll setDocumentView:_found];
    [content addSubview:scroll];
    _searching = label([self text:@"Searching for printers…"], NSMakeRect(20, 156, 480, 18), NSLeftTextAlignment);
    [_searching setFont:[NSFont systemFontOfSize:11]];
    [content addSubview:_searching];

    /* Or a printer that is not announced: its address (a host, host:port
       or a URI), reached by IPP. */
    [content addSubview:label([self text:@"Address:"], NSMakeRect(20, 124, 90, 20), NSRightTextAlignment)];
    _addAddress = [[NSTextField alloc] initWithFrame:NSMakeRect(116, 122, 384, 22)];
    [_addAddress setDelegate:self];
    [content addSubview:_addAddress];
    [content addSubview:label([self text:@"Name:"], NSMakeRect(20, 94, 90, 20), NSRightTextAlignment)];
    _addName = [[NSTextField alloc] initWithFrame:NSMakeRect(116, 92, 384, 22)];
    [content addSubview:_addName];
    [content addSubview:label([self text:@"Location:"], NSMakeRect(20, 64, 90, 20), NSRightTextAlignment)];
    _addLocation = [[NSTextField alloc] initWithFrame:NSMakeRect(116, 62, 384, 22)];
    [content addSubview:_addLocation];

    NSButton *cancel = [[NSButton alloc] initWithFrame:NSMakeRect(310, 14, 90, 28)];
    [cancel setTitle:[self text:@"Cancel"]];
    [cancel setBezelStyle:NSRoundedBezelStyle];
    [cancel setKeyEquivalent:@"\033"];
    [cancel setTarget:self];
    [cancel setAction:@selector(endAdd:)];
    [content addSubview:cancel];
    _add = [[NSButton alloc] initWithFrame:NSMakeRect(410, 14, 90, 28)];
    [_add setTitle:[self text:@"Add"]];
    [_add setBezelStyle:NSRoundedBezelStyle];
    [_add setKeyEquivalent:@"\r"];
    [_add setTag:1];
    [_add setEnabled:NO];
    [_add setTarget:self];
    [_add setAction:@selector(endAdd:)];
    [content addSubview:_add];

    /* The printers CUPS's backends find (a few seconds), off the main thread. */
    NSMutableArray *found = [NSMutableArray array];
    __weak PrintersPane *weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        cupsGetDevices(CUPS_HTTP_DEFAULT, 5, CUPS_INCLUDE_ALL, CUPS_EXCLUDE_NONE, foundDevice,
                       (__bridge void *)found);
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf foundDevices:found];
        });
    });
    [[[self mainView] window] beginSheet:_sheet completionHandler:^(NSModalResponse response) {
        [weakSelf addEnded:response];
    }];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    if ([note object] == _addAddress)
        [_add setEnabled:[[_addAddress stringValue] length] > 0 || [_found selectedRow] >= 0];
}

/* An address as a device URI: a URI as it is, else IPP Everywhere's
   path on that host. */
static NSString *addressURI(NSString *address)
{
    if ([address rangeOfString:@"://"].location != NSNotFound)
        return address;
    return [NSString stringWithFormat:@"ipp://%@/ipp/print", address];
}

- (void)foundDevices:(NSArray *)found
{
    @synchronized (found) {
        [_devices setArray:found];
    }
    [_searching setStringValue:[_devices count] ? @"" : [self text:@"No printers were found."]];
    [_found reloadData];
}

- (void)endAdd:(NSButton *)sender
{
    [[[self mainView] window] endSheet:_sheet returnCode:[sender tag] == 1 ? NSModalResponseOK : NSModalResponseCancel];
}

/* The model a printer's queue uses: IPP Everywhere for one reached by
   IPP; for another (USB, a raw network socket) one of CUPS's generic
   models by the command sets its IEEE 1284 device ID lists: PDF in a PJL
   job, or PDF as it is. nil: none of them fits (a printer that takes
   only PCL or raster data needs a driver Minkar does not have yet). */
static NSString *modelFor(NSDictionary *device)
{
    NSString *uri = [device[@"uri"] lowercaseString];
    for (NSString *scheme in @[ @"ipp://", @"ipps://", @"http://", @"https://", @"dnssd://" ])
        if ([uri hasPrefix:scheme])
            return @"everywhere";
    NSMutableSet *commands = [NSMutableSet set];
    for (NSString *field in [device[@"id"] componentsSeparatedByString:@";"]) {
        NSRange colon = [field rangeOfString:@":"];
        if (colon.location == NSNotFound)
            continue;
        NSString *key = [[field substringToIndex:colon.location] uppercaseString];
        if (![key isEqualToString:@"CMD"] && ![key isEqualToString:@"COMMAND SET"])
            continue;
        for (NSString *c in [[field substringFromIndex:colon.location + 1] componentsSeparatedByString:@","])
            [commands addObject:[[c stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
                                    uppercaseString]];
    }
    if ([commands containsObject:@"PDF"])
        return [commands containsObject:@"PJL"] ? @"generic-pdf-pjl.ppd" : @"generic-pdf.ppd";
    return nil;
}

/* The chosen printer added with its model (modelFor), its name and
   location as typed. */
- (void)addEnded:(NSModalResponse)response
{
    NSInteger row = [_found selectedRow];
    NSString *address = [[_addAddress stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    [_sheet orderOut:nil];
    NSDictionary *d = nil;
    if ([address length] > 0)
        d = @{ @"uri": addressURI(address), @"info": address };
    else if (row >= 0 && (NSUInteger)row < [_devices count])
        d = _devices[(NSUInteger)row];
    if (response != NSModalResponseOK || d == nil) {
        _sheet = nil;
        return;
    }
    NSString *name = [[_addName stringValue] length] ? [_addName stringValue] : d[@"info"];
    NSString *model = modelFor(d);
    if (model == nil) {
        os_log_info(OS_LOG_DEFAULT, "printers: no model for %{public}s (%{public}s)", [d[@"uri"] UTF8String],
                    [d[@"id"] ?: @"" UTF8String]);
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:[self text:@"The printer “%@” could not be added."], name]];
        [alert setInformativeText:[self text:@"It needs a printer driver Minkar does not have yet."]];
        [alert runModal];
        _sheet = nil;
        return;
    }
    NSMutableArray *arguments = [@[ @"-p", queueName(name), @"-E", @"-v", d[@"uri"], @"-m", model,
                                    @"-D", name ] mutableCopy];
    if ([[_addLocation stringValue] length])
        [arguments addObjectsFromArray:@[ @"-L", [_addLocation stringValue] ]];
    int status = paneRunTool(_lock, @"/usr/sbin/lpadmin", arguments);
    os_log_info(OS_LOG_DEFAULT, "printers: added %{public}s (%{public}s): %d", [queueName(name) UTF8String],
                [d[@"uri"] UTF8String], status);
    if (status != 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:[self text:@"The printer “%@” could not be added."], name]];
        [alert runModal];
    }
    _sheet = nil;
    [self reload];
}

@end
