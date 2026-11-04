/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock pane (ROADMAP Phase 5, item 6): Size, Magnification, Position
 * on screen and Automatically hide and show the Dock, written to the
 * Dock's domain (org.minkar.dock) with the reference platform's keys; the
 * Dock reads them again when org.minkar.dock.prefs is posted (notify(3)),
 * so each change shows at once. The view is made in code. Atlas's
 * provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>

@interface DockPane : NSPreferencePane
@end

#define kDockDomain CFSTR("org.minkar.dock")

static const CGFloat kWidth = 668, kHeight = 250;

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

@implementation DockPane {
    NSSlider *_size, *_large;
    NSButton *_magnify, *_left, *_bottom, *_right, *_autohide;
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

    [view addSubview:label([self text:@"Size:"], NSMakeRect(20, 196, 200, 20))];
    _size = slider(NSMakeRect(232, 194, 300, 24), number(CFSTR("tilesize"), 48), self, @selector(changed:));
    [view addSubview:_size];

    _magnify = button([self text:@"Magnification:"], NSMakeRect(90, 156, 140, 22), NSSwitchButton, self, @selector(changed:));
    [_magnify setState:flag(CFSTR("magnification")) ? NSOnState : NSOffState];
    [view addSubview:_magnify];
    _large = slider(NSMakeRect(232, 154, 300, 24), number(CFSTR("largesize"), 64), self, @selector(changed:));
    [view addSubview:_large];

    [view addSubview:label([self text:@"Position on screen:"], NSMakeRect(20, 112, 200, 20))];
    _left = button([self text:@"Left"], NSMakeRect(232, 110, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    _bottom = button([self text:@"Bottom"], NSMakeRect(322, 110, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    _right = button([self text:@"Right"], NSMakeRect(412, 110, 90, 22), NSRadioButton, self, @selector(choosePosition:));
    CFStringRef orientation = CFPreferencesCopyAppValue(CFSTR("orientation"), kDockDomain);
    NSString *where = orientation && CFGetTypeID(orientation) == CFStringGetTypeID() ?
        (__bridge NSString *)orientation : @"bottom";
    [_left setState:[where isEqualToString:@"left"] ? NSOnState : NSOffState];
    [_right setState:[where isEqualToString:@"right"] ? NSOnState : NSOffState];
    [_bottom setState:[_left state] == NSOffState && [_right state] == NSOffState ? NSOnState : NSOffState];
    if (orientation != NULL)
        CFRelease(orientation);
    for (NSButton *b in @[ _left, _bottom, _right ])
        [view addSubview:b];

    _autohide = button([self text:@"Automatically hide and show the Dock"], NSMakeRect(232, 64, 360, 22),
                       NSSwitchButton, self, @selector(changed:));
    [_autohide setState:flag(CFSTR("autohide")) ? NSOnState : NSOffState];
    [view addSubview:_autohide];

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
    CFPreferencesAppSynchronize(kDockDomain);
    CFRelease(tileNumber);
    CFRelease(largeNumber);
    notify_post("org.minkar.dock.prefs");
    os_log_info(OS_LOG_DEFAULT, "dock tilesize=%d largesize=%d magnification=%d autohide=%d orientation=%{public}s",
                tile, large, [_magnify state] == NSOnState, [_autohide state] == NSOnState, [orientation UTF8String]);
}

@end
