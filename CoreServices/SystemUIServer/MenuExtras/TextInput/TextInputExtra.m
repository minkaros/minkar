/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Input menu (the Keyboard pane's "Show Input menu in menu bar"): the
 * input source in use, by its short name (US, ES...), and a menu of the
 * enabled ones, the one in use checked; choosing one selects it (Carbon's
 * TISSelectInputSource). It follows the selection made elsewhere
 * (kTISNotifySelectedKeyboardInputSourceChanged).
 */

#import "MenuExtra.h"
#include <Carbon/Carbon.h>
#include <os/log.h>

#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[TextInputExtra class]] localizedStringForKey:(key) value:@"" table:nil]

@interface TextInputExtra : NSObject <MenuExtra, NSMenuDelegate>
@end

/* A source's short name: its layout's code (the last part of its ID). */
static NSString *shortName(TISInputSourceRef source)
{
    NSString *ident = (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    NSString *code = [[ident componentsSeparatedByString:@"."] lastObject];
    code = [[code componentsSeparatedByString:@"-"] firstObject];
    if ([code isEqualToString:@"latam"])
        return @"LA";
    if ([code isEqualToString:@"gb"])
        return @"UK";
    return [code length] ? [code uppercaseString] : @"?";
}

@implementation TextInputExtra {
    NSStatusItem *_item;
    NSMenu *_menu;
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    _menu = [[NSMenu alloc] initWithTitle:@"Input"];
    [_menu setDelegate:self];
    [_menu setAutoenablesItems:NO];
    [_item setMenu:_menu];
    [self showCurrent];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(selectionChanged:)
        name:(__bridge NSString *)kTISNotifySelectedKeyboardInputSourceChanged object:nil];
    return self;
}

- (void)showCurrent
{
    TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
    NSString *title = current != NULL ? shortName(current) : @"?";
    [[_item button] setTitle:title];
    if (current != NULL)
        CFRelease(current);
    os_log_info(OS_LOG_DEFAULT, "input menu shows %{public}s", [title UTF8String]);
}

- (void)selectionChanged:(NSNotification *)note
{
    [self showCurrent];
}

/* Built when it opens: the enabled sources there are now. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    [menu removeAllItems];
    NSArray *sources = CFBridgingRelease(TISCreateInputSourceList(NULL, false));
    for (id object in sources) {
        TISInputSourceRef source = (__bridge TISInputSourceRef)object;
        NSString *name = (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyLocalizedName);
        NSMenuItem *item = [menu addItemWithTitle:name action:@selector(choose:) keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:object];
        if (CFBooleanGetValue(TISGetInputSourceProperty(source, kTISPropertyInputSourceIsSelected)))
            [item setState:NSOnState];
    }
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *prefs = [menu addItemWithTitle:NSLocalizedString(@"Open Keyboard Preferences…", nil)
                                        action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
}

- (void)choose:(NSMenuItem *)sender
{
    OSStatus err = TISSelectInputSource((__bridge TISInputSourceRef)[sender representedObject]);
    os_log_info(OS_LOG_DEFAULT, "input menu chose %{public}s: %d", [[sender title] UTF8String], (int)err);
    [self showCurrent];
}

- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Keyboard.prefPane"]];
}

@end
