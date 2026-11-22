/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock pane (ROADMAP Phase 5, item 6), with the reference platform's
 * settings and keys: Size, Magnification, Position on screen, Minimize
 * windows using (mineffect), Minimize windows into application icon,
 * Animate opening applications (launchanim), Automatically hide and show
 * the Dock, Show indicators for open applications and Show recent
 * applications, written to the Dock's domain (org.minkar.dock); the Dock
 * reads them again when org.minkar.dock.prefs is posted (notify(3)), so
 * each change shows at once. Two are the windows' and go to the global
 * domain, where AppKit reads them: Prefer tabs when opening documents
 * (AppleWindowTabbingMode) and Double-click a window's title bar to
 * (AppleActionOnDoubleClick: Maximize, Minimize or None). The view is made
 * in code. */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>

@interface DockPane : NSPreferencePane
@end

#define kDockDomain CFSTR("org.minkar.dock")

static const CGFloat kWidth = 668, kHeight = 420;

static NSTextField *label(NSString *text, NSRect frame)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:NSRightTextAlignment];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

static double number(CFStringRef key, double fallback)
{
    CFPropertyListRef value = CFPreferencesCopyAppValue(key, kDockDomain);
    double result = fallback;

    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberDoubleType, &result);
    if (value != NULL)
        CFRelease(value);
    return result;
}

static BOOL flag(CFStringRef key)
{
    return CFPreferencesGetAppBooleanValue(key, kDockDomain, NULL);
}

/* A setting that is on when absent (the reference's default). */
static BOOL flagOn(CFStringRef key)
{
    Boolean valid = false;
    Boolean value = CFPreferencesGetAppBooleanValue(key, kDockDomain, &valid);
    return valid ? value : YES;
}

static NSString *string(CFStringRef key, CFStringRef domain, NSString *fallback)
{
    CFPropertyListRef value = CFPreferencesCopyAppValue(key, domain);
    NSString *result = fallback;

    if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID())
        result = [(__bridge NSString *)value copy];
    if (value != NULL)
        CFRelease(value);
    return result;
}

@implementation DockPane {
    NSSlider *_size, *_large;
    NSButton *_magnify, *_left, *_bottom, *_right, *_autohide;
    NSButton *_doubleClick, *_intoApp, *_animate, *_indicators, *_recents;
    NSPopUpButton *_effect, *_tabs, *_doubleClickAction;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

static NSButton *button(NSString *title, NSRect frame, NSButtonType type, id target, SEL action)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:type];
    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    return b;
}

static NSPopUpButton *popup(NSArray<NSString *> *titles, NSInteger selected, NSRect frame, id target, SEL action)
{
    NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO];
    [p addItemsWithTitles:titles];
    [p selectItemAtIndex:selected];
    [p setTarget:target];
    [p setAction:action];
    return p;
}

static NSSlider *slider(NSRect frame, double value, id target, SEL action)
{
    NSSlider *s = [[NSSlider alloc] initWithFrame:frame];
    [s setMinValue:16];
    [s setMaxValue:128];
    [s setDoubleValue:value];
    [s setContinuous:NO];
    [s setTarget:target];
    [s setAction:action];
    return s;
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    CFPreferencesAppSynchronize(kDockDomain);
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    CGFloat y = kHeight - 54;

    [view addSubview:label([self text:@"Size:"], NSMakeRect(10, y + 2, 280, 20))];
    _size = slider(NSMakeRect(300, y, 300, 24), number(CFSTR("tilesize"), 48), self, @selector(changed:));
    [view addSubview:_size];
    y -= 40;

    _magnify = button([self text:@"Magnification:"], NSMakeRect(158, y + 2, 140, 22), NSSwitchButton, self, @selector(changed:));
    [_magnify setState:flag(CFSTR("magnification")) ? NSOnState : NSOffState];
    [view addSubview:_magnify];
    _large = slider(NSMakeRect(300, y, 300, 24), number(CFSTR("largesize"), 64), self, @selector(changed:));
    [view addSubview:_large];
    y -= 42;

    [view addSubview:label([self text:@"Position on screen:"], NSMakeRect(10, y + 2, 280, 20))];
    _left = button([self text:@"Left"], NSMakeRect(300, y, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    _bottom = button([self text:@"Bottom"], NSMakeRect(390, y, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    _right = button([self text:@"Right"], NSMakeRect(480, y, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    NSString *where = string(CFSTR("orientation"), kDockDomain, @"bottom");
    [_left setState:[where isEqualToString:@"left"] ? NSOnState : NSOffState];
    [_right setState:[where isEqualToString:@"right"] ? NSOnState : NSOffState];
    [_bottom setState:[_left state] == NSOffState && [_right state] == NSOffState ? NSOnState : NSOffState];
    for (NSButton *b in @[ _left, _bottom, _right ])
        [view addSubview:b];
    y -= 38;

    [view addSubview:label([self text:@"Minimize windows using:"], NSMakeRect(10, y + 3, 280, 20))];
    _effect = popup(@[ [self text:@"Genie effect"], [self text:@"Scale effect"] ],
                    [string(CFSTR("mineffect"), kDockDomain, @"genie") isEqualToString:@"scale"] ? 1 : 0,
                    NSMakeRect(300, y, 180, 26), self, @selector(changed:));
    [view addSubview:_effect];
    y -= 34;

    [view addSubview:label([self text:@"Prefer tabs when opening documents:"], NSMakeRect(10, y + 3, 280, 20))];
    NSString *tabbing = string(CFSTR("AppleWindowTabbingMode"), kCFPreferencesAnyApplication, @"fullscreen");
    _tabs = popup(@[ [self text:@"Always"], [self text:@"In Full Screen Only"], [self text:@"Manually"] ],
                  [tabbing isEqualToString:@"always"] ? 0 : [tabbing isEqualToString:@"manual"] ? 2 : 1,
                  NSMakeRect(300, y, 180, 26), self, @selector(changed:));
    [view addSubview:_tabs];
    y -= 38;

    NSString *action = string(CFSTR("AppleActionOnDoubleClick"), kCFPreferencesAnyApplication, @"Maximize");
    _doubleClick = button([self text:@"Double-click a window’s title bar to"], NSMakeRect(300, y + 2, 250, 22),
                          NSSwitchButton, self, @selector(changed:));
    [_doubleClick setState:[action isEqualToString:@"None"] ? NSOffState : NSOnState];
    [view addSubview:_doubleClick];
    _doubleClickAction = popup(@[ [self text:@"zoom"], [self text:@"minimize"] ],
                               [action isEqualToString:@"Minimize"] ? 1 : 0,
                               NSMakeRect(552, y, 104, 26), self, @selector(changed:));
    [_doubleClickAction setEnabled:[_doubleClick state] == NSOnState];
    [view addSubview:_doubleClickAction];
    y -= 28;

    struct { NSButton *__strong *button; NSString *title; BOOL on; } checks[] = {
        { &_intoApp, @"Minimize windows into application icon", flag(CFSTR("minimize-to-application")) },
        { &_animate, @"Animate opening applications", flagOn(CFSTR("launchanim")) },
        { &_autohide, @"Automatically hide and show the Dock", flag(CFSTR("autohide")) },
        { &_indicators, @"Show indicators for open applications", flagOn(CFSTR("show-process-indicators")) },
        { &_recents, @"Show recent applications in Dock", flagOn(CFSTR("show-recents")) },
    };
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        *checks[i].button = button([self text:checks[i].title], NSMakeRect(300, y, 360, 22), NSSwitchButton,
                                   self, @selector(changed:));
        [*checks[i].button setState:checks[i].on ? NSOnState : NSOffState];
        [view addSubview:*checks[i].button];
        y -= 26;
    }

    [self setMainView:view];
    return view;
}

- (void)choosePosition:(NSButton *)sender
{
    for (NSButton *b in @[ _left, _bottom, _right ])
        [b setState:b == sender ? NSOnState : NSOffState];
    [self changed:sender];
}

/* Every setting written, then the Dock told. */
- (void)changed:(id)sender
{
    int tile = (int)lround([_size doubleValue]), large = (int)lround([_large doubleValue]);
    CFNumberRef tileNumber = CFNumberCreate(NULL, kCFNumberIntType, &tile);
    CFNumberRef largeNumber = CFNumberCreate(NULL, kCFNumberIntType, &large);
    NSString *orientation = [_left state] == NSOnState ? @"left" : [_right state] == NSOnState ? @"right" : @"bottom";

    CFPreferencesSetAppValue(CFSTR("tilesize"), tileNumber, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("largesize"), largeNumber, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("magnification"), [_magnify state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("autohide"), [_autohide state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("orientation"), (__bridge CFStringRef)orientation, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("mineffect"), [_effect indexOfSelectedItem] == 1 ? CFSTR("scale") : CFSTR("genie"), kDockDomain);
    CFPreferencesSetAppValue(CFSTR("minimize-to-application"), [_intoApp state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("launchanim"), [_animate state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("show-process-indicators"), [_indicators state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesSetAppValue(CFSTR("show-recents"), [_recents state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kDockDomain);
    CFPreferencesAppSynchronize(kDockDomain);

    /* The windows' two settings, for every app. */
    NSInteger tabs = [_tabs indexOfSelectedItem];
    CFPreferencesSetAppValue(CFSTR("AppleWindowTabbingMode"),
                             tabs == 0 ? CFSTR("always") : tabs == 2 ? CFSTR("manual") : CFSTR("fullscreen"),
                             kCFPreferencesAnyApplication);
    CFPreferencesSetAppValue(CFSTR("AppleActionOnDoubleClick"),
                             [_doubleClick state] == NSOffState ? CFSTR("None") :
                             [_doubleClickAction indexOfSelectedItem] == 1 ? CFSTR("Minimize") : CFSTR("Maximize"),
                             kCFPreferencesAnyApplication);
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    [_doubleClickAction setEnabled:[_doubleClick state] == NSOnState];
    CFRelease(tileNumber);
    CFRelease(largeNumber);
    notify_post("org.minkar.dock.prefs");
    os_log_info(OS_LOG_DEFAULT, "dock tilesize=%d largesize=%d magnification=%d autohide=%d orientation=%{public}s "
                "mineffect=%ld intoapp=%d launchanim=%d indicators=%d recents=%d tabs=%ld doubleclick=%d/%ld",
                tile, large, [_magnify state] == NSOnState, [_autohide state] == NSOnState, [orientation UTF8String],
                (long)[_effect indexOfSelectedItem], [_intoApp state] == NSOnState, [_animate state] == NSOnState,
                [_indicators state] == NSOnState, [_recents state] == NSOnState, (long)tabs,
                [_doubleClick state] == NSOnState, (long)[_doubleClickAction indexOfSelectedItem]);
}

@end
