/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Displays pane (ROADMAP Phase 5, item 6; gate M5: the resolution
 * changes from System Preferences): each active display, its size, and a
 * Resolution menu with its modes (Quartz Display Services); a choice is
 * applied at once and kept (kCGConfigurePermanently). The view is made in
 * code. Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <os/log.h>

@interface DisplaysPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kRow = 92;

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
    NSMutableArray<NSPopUpButton *> *_menus;
    NSMutableArray<NSNumber *> *_displays;
    NSMutableArray<NSArray *> *_modes;        /* per display: CGDisplayModeRefs */
    NSMutableArray<NSTextField *> *_sizes;    /* per display: its size now */
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

static NSString *modeTitle(CGDisplayModeRef mode)
{
    double hz = CGDisplayModeGetRefreshRate(mode);
    NSString *size = [NSString stringWithFormat:@"%zu × %zu", CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode)];
    return hz > 0 ? [NSString stringWithFormat:@"%@ (%.0f Hz)", size, hz] : size;
}

- (NSView *)loadMainView
{
    CGDirectDisplayID ids[16];
    uint32_t count = 0;

    CGGetActiveDisplayList(16, ids, &count);
    _menus = [NSMutableArray array];
    _displays = [NSMutableArray array];
    _modes = [NSMutableArray array];
    _sizes = [NSMutableArray array];

    CGFloat height = 40 + MAX(count, 1) * kRow;
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, height)];
    for (uint32_t i = 0; i < count; i++) {
        CGDirectDisplayID display = ids[i];
        CGFloat top = height - 24 - i * kRow;
        CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
        NSString *name = CGDisplayIsMain(display) ? [self text:@"Main Display"] :
            [NSString stringWithFormat:[self text:@"Display %u"], i + 1];
        NSString *size = [NSString stringWithFormat:@"%zu × %zu",
                          CGDisplayPixelsWide(display), CGDisplayPixelsHigh(display)];
        [view addSubview:label(name, NSMakeRect(40, top - 20, 300, 20), YES)];
        NSTextField *sizeLabel = label(size, NSMakeRect(40, top - 42, 300, 20), NO);
        [view addSubview:sizeLabel];
        [_sizes addObject:sizeLabel];
        [view addSubview:label([self text:@"Resolution:"], NSMakeRect(300, top - 20, 100, 20), NO)];

        NSPopUpButton *menu = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(400, top - 24, 220, 26) pullsDown:NO];
        NSMutableArray *usable = [NSMutableArray array];
        NSArray *all = (__bridge_transfer NSArray *)CGDisplayCopyAllDisplayModes(display, NULL);
        for (id m in all)
            if (CGDisplayModeIsUsableForDesktopGUI((__bridge CGDisplayModeRef)m))
                [usable addObject:m];
        [usable sortUsingComparator:^NSComparisonResult(id a, id b) {
            size_t wa = CGDisplayModeGetWidth((__bridge CGDisplayModeRef)a) * CGDisplayModeGetHeight((__bridge CGDisplayModeRef)a);
            size_t wb = CGDisplayModeGetWidth((__bridge CGDisplayModeRef)b) * CGDisplayModeGetHeight((__bridge CGDisplayModeRef)b);
            return wa == wb ? NSOrderedSame : wa > wb ? NSOrderedAscending : NSOrderedDescending;
        }];
        NSInteger selected = -1;
        for (NSUInteger m = 0; m < [usable count]; m++) {
            CGDisplayModeRef mode = (__bridge CGDisplayModeRef)usable[m];
            [menu addItemWithTitle:modeTitle(mode)];
            if (current != NULL && CGDisplayModeGetIODisplayModeID(mode) == CGDisplayModeGetIODisplayModeID(current))
                selected = (NSInteger)m;
        }
        if (selected >= 0)
            [menu selectItemAtIndex:selected];
        if (current != NULL)
            CGDisplayModeRelease(current);
        [menu setTag:(NSInteger)i];
        [menu setTarget:self];
        [menu setAction:@selector(chooseMode:)];
        [view addSubview:menu];
        [_menus addObject:menu];
        [_displays addObject:@(display)];
        [_modes addObject:usable];
    }
    if (count == 0)
        [view addSubview:label([self text:@"No display"], NSMakeRect(40, height - 44, 300, 20), NO)];
    [self setMainView:view];
    return view;
}

- (void)chooseMode:(NSPopUpButton *)sender
{
    NSUInteger i = (NSUInteger)[sender tag];
    NSInteger index = [sender indexOfSelectedItem];
    CGDisplayConfigRef config;

    if (i >= [_modes count] || index < 0 || (NSUInteger)index >= [_modes[i] count])
        return;
    CGDirectDisplayID display = [_displays[i] unsignedIntValue];
    CGDisplayModeRef mode = (__bridge CGDisplayModeRef)_modes[i][(NSUInteger)index];
    if (CGBeginDisplayConfiguration(&config) != kCGErrorSuccess)
        return;
    CGConfigureDisplayWithDisplayMode(config, display, mode, NULL);
    CGError err = CGCompleteDisplayConfiguration(config, kCGConfigurePermanently);
    [_sizes[i] setStringValue:[NSString stringWithFormat:@"%zu × %zu",
                               CGDisplayPixelsWide(display), CGDisplayPixelsHigh(display)]];
    os_log_info(OS_LOG_DEFAULT, "mode %zux%zu on display %u: %d", CGDisplayModeGetWidth(mode),
                CGDisplayModeGetHeight(mode), display, err);
}

@end
