/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "SMWindowController.h"
#import "SMGraphView.h"
#include <signal.h>

#define INTERVAL 2.0

static NSString *bytes(double value)
{
    static NSString *const units[] = { @"bytes", @"KB", @"MB", @"GB", @"TB" };
    int u = 0;

    while (value >= 1000 && u < 4) {
        value /= 1000;
        u++;
    }
    if (u == 0)
        return [NSString stringWithFormat:@"%.0f %@", value, NSLocalizedString(units[0], nil)];
    return [NSString stringWithFormat:value < 10 ? @"%.1f %@" : @"%.0f %@", value, units[u]];
}

static NSString *rate(double value)
{
    return [NSString stringWithFormat:NSLocalizedString(@"%@/s", nil), bytes(value)];
}

/* The columns of each tab: identifier (the SMProcess key), title, width. */
static NSArray *columnsForTab(SMTab tab)
{
    NSArray *name = @[ @"name", NSLocalizedString(@"Process Name", nil), @220 ];
    NSArray *pid = @[ @"pid", NSLocalizedString(@"PID", nil), @60 ];
    NSArray *user = @[ @"user", NSLocalizedString(@"User", nil), @90 ];

    switch (tab) {
    case SMTabCPU:
        return @[ name, @[ @"cpuPercent", NSLocalizedString(@"% CPU", nil), @70 ],
            @[ @"cpuTime", NSLocalizedString(@"CPU Time", nil), @80 ],
            @[ @"threads", NSLocalizedString(@"Threads", nil), @60 ], pid, user ];
    case SMTabMemory:
        return @[ name, @[ @"memory", NSLocalizedString(@"Memory", nil), @90 ],
            @[ @"threads", NSLocalizedString(@"Threads", nil), @60 ], pid, user ];
    case SMTabDisk:
        return @[ name, @[ @"bytesWritten", NSLocalizedString(@"Bytes Written", nil), @100 ],
            @[ @"bytesRead", NSLocalizedString(@"Bytes Read", nil), @100 ], pid, user ];
    case SMTabNetwork:
        return @[ @[ @"name", NSLocalizedString(@"Interface", nil), @120 ],
            @[ @"bytesIn", NSLocalizedString(@"Bytes Received", nil), @120 ],
            @[ @"bytesOut", NSLocalizedString(@"Bytes Sent", nil), @120 ],
            @[ @"inRate", NSLocalizedString(@"Received/s", nil), @100 ],
            @[ @"outRate", NSLocalizedString(@"Sent/s", nil), @100 ] ];
    }
    return @[];
}

@implementation SMWindowController {
    NSSegmentedControl *_tabs;
    NSSearchField *_search;
    NSTableView *_table;
    SMGraphView *_graph;
    NSTextField *_summary;
    NSTimer *_timer;
    NSString *_sortKey[4];
    BOOL _ascending[4];
    NSArray *_rows;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 780, 560)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];

    if ((self = [super initWithWindow:window]) == nil)
        return nil;
    _sampler = [SMSampler new];
    _sortKey[SMTabCPU] = @"cpuPercent";
    _sortKey[SMTabMemory] = @"memory";
    _sortKey[SMTabDisk] = @"bytesWritten";
    _sortKey[SMTabNetwork] = @"name";
    _ascending[SMTabNetwork] = YES;
    _searchString = @"";
    [self makeViews];
    [self setTab:SMTabCPU];
    [_sampler sample];
    [self refresh];
    _timer = [NSTimer scheduledTimerWithTimeInterval:INTERVAL target:self selector:@selector(tick:)
        userInfo:nil repeats:YES];
    return self;
}

- (NSButton *)toolButton:(NSString *)image action:(SEL)action tip:(NSString *)tip frame:(NSRect)frame
{
    NSButton *button = [[NSButton alloc] initWithFrame:frame];

    [button setBezelStyle:NSRoundedBezelStyle];
    [button setImage:[NSImage imageNamed:image]];
    [button setImagePosition:NSImageOnly];
    [button setToolTip:tip];
    [button setTarget:self];
    [button setAction:action];
    [button setAutoresizingMask:NSViewMinYMargin];
    return button;
}

- (void)makeViews
{
    NSWindow *window = [self window];
    NSView *content = [window contentView];
    NSRect bounds = [content bounds];
    CGFloat top = NSHeight(bounds) - 40;

    [window setTitle:NSLocalizedString(@"System Monitor", nil)];
    [window setMinSize:NSMakeSize(600, 420)];
    [window setFrameAutosaveName:@"SystemMonitor"];

    [content addSubview:[self toolButton:NSImageNameStopProgressTemplate action:@selector(stopSelectedProcess:)
        tip:NSLocalizedString(@"Quit the selected process", nil) frame:NSMakeRect(12, top, 36, 26)]];
    [content addSubview:[self toolButton:NSImageNameInfo action:@selector(showProcessInfo:)
        tip:NSLocalizedString(@"Show information about the selected process", nil) frame:NSMakeRect(52, top, 36, 26)]];

    _tabs = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(NSMidX(bounds) - 180, top, 360, 26)];
    [_tabs setSegmentCount:4];
    NSArray *labels = @[ NSLocalizedString(@"CPU", nil), NSLocalizedString(@"Memory", nil),
        NSLocalizedString(@"Disk", nil), NSLocalizedString(@"Network", nil) ];
    for (NSInteger i = 0; i < 4; i++) {
        [_tabs setLabel:labels[i] forSegment:i];
        [_tabs setWidth:88 forSegment:i];
    }
    [_tabs setTarget:self];
    [_tabs setAction:@selector(chooseTab:)];
    [_tabs setAutoresizingMask:NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin];
    [content addSubview:_tabs];

    _search = [[NSSearchField alloc] initWithFrame:NSMakeRect(NSMaxX(bounds) - 192, top + 2, 180, 22)];
    [[_search cell] setPlaceholderString:NSLocalizedString(@"Search", nil)];
    [_search setTarget:self];
    [_search setAction:@selector(searchChanged:)];
    [_search setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [content addSubview:_search];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 150, NSWidth(bounds), top - 158)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSNoBorder];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    _table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [_table setUsesAlternatingRowBackgroundColors:YES];
    [_table setDoubleAction:@selector(showProcessInfo:)];
    [_table setTarget:self];
    [scroll setDocumentView:_table];
    [content addSubview:scroll];

    _graph = [[SMGraphView alloc] initWithFrame:NSMakeRect(NSWidth(bounds) - 312, 12, 300, 126)];
    [_graph setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
    [content addSubview:_graph];

    _summary = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 12, NSWidth(bounds) - 344, 126)];
    [_summary setBezeled:NO];
    [_summary setDrawsBackground:NO];
    [_summary setEditable:NO];
    [_summary setSelectable:YES];
    [_summary setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
    [content addSubview:_summary];
}

- (void)tick:(NSTimer *)timer
{
    [_sampler sample];
    [self refresh];
}

- (void)setTab:(SMTab)tab
{
    _tab = tab;
    [_tabs setSelectedSegment:tab];
    for (NSTableColumn *c in [[_table tableColumns] copy])
        [_table removeTableColumn:c];
    for (NSArray *spec in columnsForTab(tab)) {
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:spec[0]];
        [[column headerCell] setStringValue:spec[1]];
        [column setWidth:[spec[2] doubleValue]];
        [column setEditable:NO];
        [_table addTableColumn:column];
    }
    [_graph clear];
    _graph.maximum = (tab == SMTabCPU) ? 100 : 0;
    _graph.colors = (tab == SMTabMemory) ? @[ [NSColor systemGreenColor] ] :
        @[ [NSColor controlAccentColor], [NSColor systemRedColor] ];
    [self refresh];
}

- (IBAction)chooseTab:(id)sender
{
    if ([sender isKindOfClass:[NSSegmentedControl class]])
        [self setTab:[(NSSegmentedControl *)sender selectedSegment]];
    else if ([sender isKindOfClass:[NSMenuItem class]])
        [self setTab:[(NSMenuItem *)sender tag]];
}

- (void)searchChanged:(id)sender
{
    [self setSearchString:[_search stringValue]];
}

- (void)setSearchString:(NSString *)string
{
    _searchString = [string copy] ?: @"";
    [self refresh];
}

- (void)sortByKey:(NSString *)key ascending:(BOOL)ascending
{
    _sortKey[_tab] = [key copy];
    _ascending[_tab] = ascending;
    [self refresh];
}

/* A click on a column's header sorts by it; again, the other way. */
- (void)tableView:(NSTableView *)table didClickTableColumn:(NSTableColumn *)column
{
    NSString *key = [column identifier];
    BOOL textual = [key isEqual:@"name"] || [key isEqual:@"user"];

    if ([key isEqual:_sortKey[_tab]])
        [self sortByKey:key ascending:!_ascending[_tab]];
    else
        [self sortByKey:key ascending:textual];
}

- (NSArray *)rows
{
    return _rows;
}

- (void)refresh
{
    NSArray *source = (_tab == SMTabNetwork) ? _sampler.interfaces : _sampler.processes;
    NSString *key = _sortKey[_tab];
    BOOL ascending = _ascending[_tab];
    NSInteger selected = [_table selectedRow];
    id selectedObject = (selected >= 0 && selected < (NSInteger)[_rows count]) ? _rows[selected] : nil;
    pid_t selectedPID = [selectedObject isKindOfClass:[SMProcess class]] ? [(SMProcess *)selectedObject pid] : -1;

    if ([_searchString length])
        source = [source filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id row, NSDictionary *b) {
            return [[row name] rangeOfString:_searchString options:NSCaseInsensitiveSearch].location != NSNotFound;
        }]];
    _rows = [source sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
        id va = [a valueForKey:key], vb = [b valueForKey:key];
        NSComparisonResult r = [va isKindOfClass:[NSString class]] ?
            [va localizedCaseInsensitiveCompare:vb] : [va compare:vb];
        return ascending ? r : -r;
    }];
    [_table reloadData];
    if (selectedPID > 0)
        for (NSUInteger i = 0; i < [_rows count]; i++)
            if ([_rows[i] isKindOfClass:[SMProcess class]] && [(SMProcess *)_rows[i] pid] == selectedPID) {
                [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
                break;
            }
    [self updateSummary];
}

- (void)updateSummary
{
    SMSampler *s = _sampler;
    NSString *text = @"";

    switch (_tab) {
    case SMTabCPU:
        text = [NSString stringWithFormat:NSLocalizedString(@"System: %.1f %%\nUser: %.1f %%\nIdle: %.1f %%\nProcessors: %d\nProcesses: %lu", nil),
            s.systemPercent, s.userPercent, s.idlePercent, s.processors, (unsigned long)[s.processes count]];
        [_graph addValues:@[ @(s.userPercent + s.systemPercent), @(s.systemPercent) ]];
        break;
    case SMTabMemory:
        text = [NSString stringWithFormat:NSLocalizedString(@"Physical memory: %@\nMemory used: %@\nWired memory: %@\nFree memory: %@", nil),
            bytes(s.physicalMemory), bytes(s.usedMemory), bytes(s.wiredMemory), bytes(s.freeMemory)];
        [_graph addValues:@[ @(s.usedMemory) ]];
        _graph.maximum = s.physicalMemory;
        break;
    case SMTabDisk:
        text = [NSString stringWithFormat:NSLocalizedString(@"Data read: %@\nData written: %@", nil),
            rate(s.readRate), rate(s.writeRate)];
        [_graph addValues:@[ @(s.readRate), @(s.writeRate) ]];
        break;
    case SMTabNetwork:
        text = [NSString stringWithFormat:NSLocalizedString(@"Data received: %@\nData sent: %@", nil),
            rate(s.inRate), rate(s.outRate)];
        [_graph addValues:@[ @(s.inRate), @(s.outRate) ]];
        break;
    }
    [_summary setStringValue:text];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return [_rows count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    id object = _rows[row];
    NSString *key = [column identifier];
    id value = [object valueForKey:key];

    if ([key isEqual:@"cpuPercent"])
        return [NSString stringWithFormat:@"%.1f", [value doubleValue]];
    if ([key isEqual:@"cpuTime"]) {
        double t = [value doubleValue];
        return [NSString stringWithFormat:@"%d:%05.2f", (int)(t / 60), fmod(t, 60)];
    }
    if ([key isEqual:@"memory"] || [key hasPrefix:@"bytes"])
        return bytes([value doubleValue]);
    if ([key hasSuffix:@"Rate"])
        return rate([value doubleValue]);
    return value;
}

- (SMProcess *)selectedProcess
{
    NSInteger row = [_table selectedRow];

    if (_tab == SMTabNetwork || row < 0 || row >= (NSInteger)[_rows count])
        return nil;
    return _rows[row];
}

- (BOOL)stopProcess:(pid_t)pid force:(BOOL)force
{
    if (!force) {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
        if (app != nil && [app terminate])
            return YES;
    }
    return kill(pid, force ? SIGKILL : SIGTERM) == 0;
}

- (IBAction)stopSelectedProcess:(id)sender
{
    SMProcess *process = [self selectedProcess];

    if (process == nil) {
        NSBeep();
        return;
    }
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"Are you sure you want to quit “%@”?", nil),
        process.name]];
    [alert setInformativeText:NSLocalizedString(@"Force Quit ends it at once: changes it has not saved are lost.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Quit", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Force Quit", nil)];
    NSInteger answer = [alert runModal];
    BOOL done = YES;
    if (answer == NSAlertFirstButtonReturn)
        done = [self stopProcess:process.pid force:NO];
    else if (answer == NSAlertThirdButtonReturn)
        done = [self stopProcess:process.pid force:YES];
    else
        return;
    if (!done) {
        NSAlert *failed = [[NSAlert alloc] init];
        [failed setMessageText:[NSString stringWithFormat:NSLocalizedString(@"“%@” could not be quit.", nil), process.name]];
        [failed setInformativeText:[NSString stringWithUTF8String:strerror(errno)]];
        [failed runModal];
    }
    [_sampler sample];
    [self refresh];
}

- (IBAction)showProcessInfo:(id)sender
{
    SMProcess *p = [self selectedProcess];

    if (p == nil) {
        NSBeep();
        return;
    }
    SMProcess *parent = [_sampler processWithPID:p.parentPID];
    NSDateFormatter *when = [[NSDateFormatter alloc] init];
    [when setDateStyle:NSDateFormatterMediumStyle];
    [when setTimeStyle:NSDateFormatterMediumStyle];
    NSString *info = [NSString stringWithFormat:NSLocalizedString(
        @"Process: %@ (%d)\nParent process: %@ (%d)\nUser: %@\nPath: %@\nStarted: %@\nCPU time: %.2f s\nMemory: %@\nThreads: %d\nData read: %@\nData written: %@", nil),
        p.name, p.pid, parent.name ?: @"—", p.parentPID, p.user, [p.path length] ? p.path : @"—",
        [when stringFromDate:p.startDate], p.cpuTime, bytes(p.memory), p.threads, bytes(p.bytesRead), bytes(p.bytesWritten)];

    NSPanel *panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 440, 240)
        styleMask:NSTitledWindowMask | NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];
    [panel setTitle:p.name];
    [panel setReleasedWhenClosed:NO];
    NSTextField *text = [[NSTextField alloc] initWithFrame:NSInsetRect([[panel contentView] bounds], 16, 12)];
    [text setStringValue:info];
    [text setBezeled:NO];
    [text setDrawsBackground:NO];
    [text setEditable:NO];
    [text setSelectable:YES];
    [[panel contentView] addSubview:text];
    [panel center];
    [panel makeKeyAndOrderFront:nil];
    static NSMutableArray *panels;
    if (panels == nil)
        panels = [NSMutableArray array];
    [panels addObject:panel];
}

@end
