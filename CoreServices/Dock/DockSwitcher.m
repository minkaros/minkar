/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The app switcher (Atlas.md, D3). See DockSwitcher.h.
 */

#import "DockSwitcher.h"
#include <SkyLight/SkyLight.h>
#include <notify.h>
#import "DockModel.h"
#include <os/log.h>

static const CGFloat kIcon = 64, kCell = 84, kPad = 16, kLabel = 22;

@interface DockSwitcherView : NSView
@property (nonatomic, copy) NSArray<NSRunningApplication *> *apps;
@property (nonatomic) NSUInteger selected;
@end

@implementation DockSwitcherView
- (BOOL)isOpaque { return NO; }

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill(dirty);
    [[NSColor colorWithGenericGamma22White:0.12 alpha:0.85] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:[self bounds] xRadius:18 yRadius:18] fill];
    for (NSUInteger i = 0; i < [self.apps count]; i++) {
        NSRect cell = NSMakeRect(kPad + i * kCell, kPad + kLabel, kCell, kCell);
        if (i == self.selected) {
            [[NSColor colorWithGenericGamma22White:1 alpha:0.18] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(cell, 2, 2) xRadius:12 yRadius:12] fill];
        }
        NSImage *icon = [self.apps[i] icon];
        [icon drawInRect:NSInsetRect(cell, (kCell - kIcon) / 2, (kCell - kIcon) / 2) fromRect:NSZeroRect
               operation:NSCompositeSourceOver fraction:1];
    }
    if (self.selected < [self.apps count]) {
        NSString *name = [self.apps[self.selected] localizedName] ?: @"";
        NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                                 NSForegroundColorAttributeName: [NSColor whiteColor] };
        NSSize size = [name sizeWithAttributes:attrs];
        [name drawAtPoint:NSMakePoint(NSMidX([self bounds]) - size.width / 2, kPad / 2 + 2) withAttributes:attrs];
    }
}
@end

@implementation DockSwitcher {
    NSMutableArray<NSRunningApplication *> *_recent;    /* most recently active first */
    NSPanel *_panel;
    DockSwitcherView *_view;
    NSTimer *_poll;
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _recent = [NSMutableArray array];
    NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (front != nil)
        [_recent addObject:front];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(activated:)
        name:NSWorkspaceDidActivateApplicationNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(hotKey:)
        name:@"_NSSkyLightSymbolicHotKeyNotification" object:nil];
    SLSClaimSymbolicHotKey(kSLSHotKeyAppSwitcher, 1);
    SLSClaimSymbolicHotKey(kSLSHotKeyAppSwitcherBackward, 1);
    SLSClaimSymbolicHotKey(kSLSHotKeyDockHiding, 1);
    return self;
}

- (void)activated:(NSNotification *)note
{
    NSRunningApplication *app = note.userInfo[NSWorkspaceApplicationKey];
    if (app == nil)
        return;
    [_recent removeObject:app];
    [_recent insertObject:app atIndex:0];
}

/* The regular apps running, the most recently active first, then the
   others in the order they started. */
- (NSArray<NSRunningApplication *> *)apps
{
    NSMutableArray *apps = [NSMutableArray array];
    NSArray *running = [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication *app in _recent)
        if ([running containsObject:app] && ![app isTerminated] &&
            [app activationPolicy] == NSApplicationActivationPolicyRegular)
            [apps addObject:app];
    for (NSRunningApplication *app in running)
        if (![apps containsObject:app] && ![app isTerminated] &&
            [app activationPolicy] == NSApplicationActivationPolicyRegular)
            [apps addObject:app];
    return apps;
}

- (void)hotKey:(NSNotification *)note
{
    uint32_t hotKey = [note.userInfo[@"hotKey"] unsignedIntValue];
    if (hotKey == kSLSHotKeyDockHiding) {
        /* Option-Command-D: hiding turned on or off, as the Dock pane
           does it (the preference, then its notification). */
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [defaults setBool:![defaults boolForKey:@"autohide"] forKey:@"autohide"];
        [defaults synchronize];
        notify_post(kDockPreferencesChangedNotifyName);
        return;
    }
    if (hotKey != kSLSHotKeyAppSwitcher && hotKey != kSLSHotKeyAppSwitcherBackward)
        return;
    BOOL backward = hotKey == kSLSHotKeyAppSwitcherBackward;
    if (_panel == nil || ![_panel isVisible]) {
        NSArray *apps = [self apps];
        if ([apps count] == 0)
            return;
        [self showApps:apps selected:backward ? [apps count] - 1 : MIN((NSUInteger)1, [apps count] - 1)];
        return;
    }
    NSUInteger count = [_view.apps count];
    _view.selected = backward ? (_view.selected + count - 1) % count : (_view.selected + 1) % count;
    [_view setNeedsDisplay:YES];
}

- (void)showApps:(NSArray<NSRunningApplication *> *)apps selected:(NSUInteger)selected
{
    NSRect screen = [[[NSScreen screens] firstObject] frame];
    NSSize size = NSMakeSize(2 * kPad + [apps count] * kCell, 2 * kPad + kCell + kLabel);
    NSRect frame = NSMakeRect(NSMidX(screen) - size.width / 2, NSMidY(screen) - size.height / 2, size.width, size.height);

    if (_panel == nil) {
        _panel = [[NSPanel alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless
                                              backing:NSBackingStoreBuffered defer:NO];
        [_panel setLevel:NSPopUpMenuWindowLevel];
        [_panel setOpaque:NO];
        [_panel setBackgroundColor:[NSColor clearColor]];
        [_panel setHasShadow:YES];
        [_panel setHidesOnDeactivate:NO];
        [_panel setIgnoresMouseEvents:YES];
        _view = [[DockSwitcherView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
        [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [_panel setContentView:_view];
    }
    [_panel setFrame:frame display:NO];
    _view.apps = apps;
    _view.selected = selected;
    [_view setNeedsDisplay:YES];
    [_panel orderFront:nil];
    os_log_info(OS_LOG_DEFAULT, "app switcher with %lu apps", (unsigned long)[apps count]);
    /* The Dock gets no key events (the front app does): Command let go is
       read from the input state. */
    [_poll invalidate];
    _poll = [NSTimer scheduledTimerWithTimeInterval:0.03 target:self selector:@selector(poll:) userInfo:nil repeats:YES];
}

- (void)poll:(NSTimer *)timer
{
    if ([NSEvent modifierFlags] & NSCommandKeyMask)
        return;
    [_poll invalidate];
    _poll = nil;
    [_panel orderOut:nil];
    if (_view.selected < [_view.apps count]) {
        NSRunningApplication *app = _view.apps[_view.selected];
        os_log_info(OS_LOG_DEFAULT, "switched to %{public}s", [[app localizedName] UTF8String]);
        [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }
}

@end
