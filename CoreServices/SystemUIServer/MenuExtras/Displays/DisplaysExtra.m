/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Displays Menu Extra (Atlas.md, MB1): each display with its modes,
 * the current one checked; choosing one sets it (Quartz Display Services,
 * for this session), and Displays Preferences opens the pane.
 */

#import "MenuExtra.h"
#include <CoreGraphics/CoreGraphics.h>

#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[DisplaysExtra class]] localizedStringForKey:(key) value:@"" table:nil]

@interface DisplaysExtra : NSObject <MenuExtra, NSMenuDelegate>
@end

@implementation DisplaysExtra {
    NSStatusItem *_item;
    NSMenu *_menu;
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    NSImage *icon = [[NSImage alloc] initWithContentsOfFile:@"/System/Library/MenuBarIcons/video-display.png"];
    [icon setSize:NSMakeSize(18, 18)];
    /* Black on clear: drawn in the menu bar's own color. */
    [icon setTemplate:YES];
    if (icon != nil)
        [[_item button] setImage:icon];
    else
        [[_item button] setTitle:NSLocalizedString(@"Displays", nil)];
    _menu = [[NSMenu alloc] initWithTitle:@"Displays"];
    [_menu setDelegate:self];
    [_menu setAutoenablesItems:NO];
    [_item setMenu:_menu];
    [self menuNeedsUpdate:_menu];
    return self;
}

/* Built when it opens: the displays and modes there are now. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    CGDirectDisplayID displays[16];
    uint32_t count = 0;

    [menu removeAllItems];
    CGGetActiveDisplayList(16, displays, &count);
    for (uint32_t d = 0; d < count; d++) {
        if (count > 1) {
            NSMenuItem *header = [menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Display %u", nil), d + 1]
                                                 action:NULL keyEquivalent:@""];
            [header setEnabled:NO];
        }
        CGDisplayModeRef current = CGDisplayCopyDisplayMode(displays[d]);
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(displays[d], NULL);
        for (CFIndex i = 0; modes != NULL && i < CFArrayGetCount(modes); i++) {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
            NSString *title = [NSString stringWithFormat:NSLocalizedString(@"%u × %u, %.0f Hz", nil),
                               (unsigned)CGDisplayModeGetWidth(mode), (unsigned)CGDisplayModeGetHeight(mode),
                               CGDisplayModeGetRefreshRate(mode)];
            NSMenuItem *item = [menu addItemWithTitle:title action:@selector(chooseMode:) keyEquivalent:@""];
            [item setTarget:self];
            [item setTag:(NSInteger)displays[d]];
            [item setRepresentedObject:(__bridge id)mode];
            if (current != NULL && CGDisplayModeGetIODisplayModeID(mode) == CGDisplayModeGetIODisplayModeID(current))
                [item setState:NSOnState];
        }
        if (modes != NULL)
            CFRelease(modes);
        if (current != NULL)
            CGDisplayModeRelease(current);
        [menu addItem:[NSMenuItem separatorItem]];
    }
    NSMenuItem *prefs = [menu addItemWithTitle:NSLocalizedString(@"Displays Preferences…", nil)
                                        action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
}

- (void)chooseMode:(NSMenuItem *)sender
{
    CGDisplayModeRef mode = (__bridge CGDisplayModeRef)[sender representedObject];
    CGDisplayConfigRef config;

    if (mode == NULL || CGBeginDisplayConfiguration(&config) != kCGErrorSuccess)
        return;
    CGConfigureDisplayWithDisplayMode(config, (CGDirectDisplayID)[sender tag], mode, NULL);
    CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
}

- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Displays.prefPane"]];
}

@end
