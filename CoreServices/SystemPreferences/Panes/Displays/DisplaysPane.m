/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Displays pane (ROADMAP Phase 5, item 6; gate M5: the resolution
 * changes from System Preferences), laid out as the reference's Display
 * tab: Resolution, Default for display (the mode the driver marks default
 * or native, else the display's first, the one it starts in) or Scaled,
 * which lists the display's modes, largest first; a choice is applied at
 * once and kept (kCGConfigurePermanently, Quartz Display Services). With
 * more than one display, a menu chooses which. Below, Show mirroring
 * options in the menu bar when available. The view is made in code.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <os/log.h>
#import "../MenuExtraList.h"

@interface DisplaysPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate>
@end

static const CGFloat kWidth = 668;

static NSTextField *label(NSString *text, NSRect frame, BOOL bold)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setFont:bold ? [NSFont boldSystemFontOfSize:13] : [NSFont systemFontOfSize:13]];
    return field;
}

@implementation DisplaysPane {
    NSMutableArray<NSNumber *> *_displays;
    NSPopUpButton *_which;
    NSTextField *_size;
    NSButton *_default, *_scaled;
    NSScrollView *_scroll;
    NSTableView *_table;
    NSArray *_modes;                /* the display's, largest first */
    CGDirectDisplayID _display;
    BOOL _updating;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

static NSString *modeTitle(CGDisplayModeRef mode)
{
    return [NSString stringWithFormat:@"%zu × %zu", CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode)];
}

/* The display's default mode: the one the driver marks default or native,
   else its first (the one it starts in). */
static CGDisplayModeRef defaultMode(NSArray *all)
{
    for (id m in all)
        if (CGDisplayModeGetIOFlags((__bridge CGDisplayModeRef)m) & (0x4 | 0x02000000))
            return (__bridge CGDisplayModeRef)m;
    return [all count] ? (__bridge CGDisplayModeRef)all[0] : NULL;
}

- (NSView *)loadMainView
{
    CGDirectDisplayID ids[16];
    uint32_t count = 0;

    CGGetActiveDisplayList(16, ids, &count);
    _displays = [NSMutableArray array];
    for (uint32_t i = 0; i < count; i++)
        [_displays addObject:@(ids[i])];

    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, 330)];
    if (count > 1) {
        _which = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(40, 290, 220, 26) pullsDown:NO];
        for (uint32_t i = 0; i < count; i++)
            [_which addItemWithTitle:CGDisplayIsMain(ids[i]) ? [self text:@"Main Display"] :
                [NSString stringWithFormat:[self text:@"Display %u"], i + 1]];
        [_which setTarget:self];
        [_which setAction:@selector(displayChosen:)];
        [view addSubview:_which];
    }
    _size = label(@"", NSMakeRect(40, 262, 200, 20), NO);
    [view addSubview:_size];
    NSTextField *res = label([self text:@"Resolution:"], NSMakeRect(130, 236, 100, 20), NO);
    [res setAlignment:NSRightTextAlignment];
    [view addSubview:res];
    _default = [[NSButton alloc] initWithFrame:NSMakeRect(240, 236, 300, 20)];
    [_default setButtonType:NSRadioButton];
    [_default setTitle:[self text:@"Default for display"]];
    [_default setTarget:self];
    [_default setAction:@selector(chooseDefault:)];
    [view addSubview:_default];
    _scaled = [[NSButton alloc] initWithFrame:NSMakeRect(240, 212, 300, 20)];
    [_scaled setButtonType:NSRadioButton];
    [_scaled setTitle:[self text:@"Scaled"]];
    [_scaled setTarget:self];
    [_scaled setAction:@selector(chooseScaled:)];
    [view addSubview:_scaled];

    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(258, 50, 260, 156)];
    [_scroll setBorderType:NSBezelBorder];
    [_scroll setHasVerticalScroller:YES];
    _table = [[NSTableView alloc] initWithFrame:[[_scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"mode"];
    [column setWidth:256];
    [column setEditable:NO];
    [_table addTableColumn:column];
    [_table setHeaderView:nil];
    [_table setRowHeight:20];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [_scroll setDocumentView:_table];
    [view addSubview:_scroll];

    if (count == 0)
        [view addSubview:label([self text:@"No display"], NSMakeRect(40, 262, 300, 20), NO)];
    /* As on the reference: the Displays Menu Extra in the menu bar or not. */
    NSButton *show = [[NSButton alloc] initWithFrame:NSMakeRect(40, 12, 560, 22)];
    [show setButtonType:NSSwitchButton];
    [show setTitle:[self text:@"Show mirroring options in the menu bar when available"]];
    [show setState:MenuExtraIsShown(@"Displays") ? NSOnState : NSOffState];
    [show setTarget:self];
    [show setAction:@selector(showInMenuBar:)];
    [view addSubview:show];
    if (count > 0)
        [self showDisplay:ids[0]];
    [self setMainView:view];
    return view;
}

/* The display's modes, size and choice. */
- (void)showDisplay:(CGDirectDisplayID)display
{
    _display = display;
    NSMutableArray *usable = [NSMutableArray array];
    NSArray *all = (__bridge_transfer NSArray *)CGDisplayCopyAllDisplayModes(display, NULL);
    for (id m in all)
        if (CGDisplayModeIsUsableForDesktopGUI((__bridge CGDisplayModeRef)m))
            [usable addObject:m];
    CGDisplayModeRef def = defaultMode(usable);
    [usable sortUsingComparator:^NSComparisonResult(id a, id b) {
        size_t wa = CGDisplayModeGetWidth((__bridge CGDisplayModeRef)a) * CGDisplayModeGetHeight((__bridge CGDisplayModeRef)a);
        size_t wb = CGDisplayModeGetWidth((__bridge CGDisplayModeRef)b) * CGDisplayModeGetHeight((__bridge CGDisplayModeRef)b);
        return wa == wb ? NSOrderedSame : wa > wb ? NSOrderedAscending : NSOrderedDescending;
    }];
    _modes = usable;
    [_table reloadData];

    CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
    int32_t currentID = current != NULL ? CGDisplayModeGetIODisplayModeID(current) : -1;
    if (current != NULL)
        CGDisplayModeRelease(current);
    BOOL isDefault = def != NULL && CGDisplayModeGetIODisplayModeID(def) == currentID;
    /* Scaled stays chosen once the user chose it. */
    if (![_scaled state] || isDefault) {
        [_default setState:isDefault ? NSOnState : NSOffState];
        [_scaled setState:isDefault ? NSOffState : NSOnState];
    }
    [_scroll setHidden:[_scaled state] != NSOnState];
    _updating = YES;
    for (NSUInteger i = 0; i < [_modes count]; i++)
        if (CGDisplayModeGetIODisplayModeID((__bridge CGDisplayModeRef)_modes[i]) == currentID)
            [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
    _updating = NO;
    [_size setStringValue:[NSString stringWithFormat:@"%zu × %zu",
                           CGDisplayPixelsWide(display), CGDisplayPixelsHigh(display)]];
}

- (void)displayChosen:(NSPopUpButton *)sender
{
    NSInteger i = [sender indexOfSelectedItem];
    if (i >= 0 && (NSUInteger)i < [_displays count])
        [self showDisplay:[_displays[(NSUInteger)i] unsignedIntValue]];
}

- (void)applyMode:(CGDisplayModeRef)mode
{
    CGDisplayConfigRef config;

    if (mode == NULL || CGBeginDisplayConfiguration(&config) != kCGErrorSuccess)
        return;
    CGConfigureDisplayWithDisplayMode(config, _display, mode, NULL);
    CGError err = CGCompleteDisplayConfiguration(config, kCGConfigurePermanently);
    os_log_info(OS_LOG_DEFAULT, "mode %zux%zu on display %u: %d", CGDisplayModeGetWidth(mode),
                CGDisplayModeGetHeight(mode), _display, err);
    [self showDisplay:_display];
}

- (void)chooseDefault:(id)sender
{
    [_scaled setState:NSOffState];
    [_default setState:NSOnState];
    [self applyMode:defaultMode(_modes)];
}

- (void)chooseScaled:(id)sender
{
    [_default setState:NSOffState];
    [_scaled setState:NSOnState];
    [_scroll setHidden:NO];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_modes count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    return modeTitle((__bridge CGDisplayModeRef)_modes[(NSUInteger)row]);
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    NSInteger row = [_table selectedRow];
    if (_updating || row < 0 || (NSUInteger)row >= [_modes count])
        return;
    [self applyMode:(__bridge CGDisplayModeRef)_modes[(NSUInteger)row]];
}

- (void)showInMenuBar:(NSButton *)sender
{
    MenuExtraSetShown(@"Displays", [sender state] == NSOnState);
    os_log_info(OS_LOG_DEFAULT, "displays menu extra shown=%d", [sender state] == NSOnState);
}

@end
