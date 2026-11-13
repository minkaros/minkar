/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Mouse pane: the primary mouse button (left, or right for the
 * left-handed) and the double-click speed (150 ms to 2 s between the
 * clicks), set in the window server (SLSSetMouseSettings), which counts
 * the clicks, swaps the buttons and keeps the settings. The reference
 * platform's global keys (com.apple.mouse.swapLeftRightButton,
 * com.apple.mouse.doubleClickThreshold) are written too, for apps that
 * read them. Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <SkyLight/SkyLight.h>
#include <math.h>
#include <os/log.h>

@interface MousePane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 200;
static const double kFastest = 150, kSlowest = 2000;	/* ms */

static NSTextField *label(NSString *text, NSRect frame, CGFloat size, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    [field setFont:[NSFont systemFontOfSize:size]];
    return field;
}

/* The slider runs Slow (left) to Fast (right) on a logarithmic scale: the
   short intervals, where the difference is felt, get most of its length. */
static double sliderValue(uint32_t ms)
{
    return log(kSlowest / ms) / log(kSlowest / kFastest);
}

static uint32_t interval(double value)
{
    return (uint32_t)lround(kSlowest / pow(kSlowest / kFastest, value));
}

@implementation MousePane {
    NSButton *_left, *_right;
    NSSlider *_speed;
    SLSMouseSettings _settings;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (NSButton *)radio:(NSString *)title frame:(NSRect)frame
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:NSButtonTypeRadio];
    [b setTitle:[self text:title]];
    [b setTarget:self];
    [b setAction:@selector(primaryChanged:)];
    return b;
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    if (SLSGetMouseSettings(&_settings) != kSLSErrorSuccess) {
        _settings.swapButtons = 0;
        _settings.doubleClickInterval = 500;
    }

    /* Primary mouse button: Left, Right. */
    [view addSubview:label([self text:@"Primary mouse button:"], NSMakeRect(40, 142, 220, 20), 13,
                           NSRightTextAlignment)];
    _left = [self radio:@"Left" frame:NSMakeRect(270, 142, 120, 20)];
    _right = [self radio:@"Right" frame:NSMakeRect(390, 142, 120, 20)];
    [_left setState:_settings.swapButtons ? NSControlStateValueOff : NSControlStateValueOn];
    [_right setState:_settings.swapButtons ? NSControlStateValueOn : NSControlStateValueOff];
    [view addSubview:_left];
    [view addSubview:_right];

    /* Double-Click Speed: Slow to Fast. */
    [view addSubview:label([self text:@"Double-Click Speed"], NSMakeRect(214, 96, 240, 18), 13,
                           NSCenterTextAlignment)];
    _speed = [[NSSlider alloc] initWithFrame:NSMakeRect(214, 66, 240, 24)];
    [_speed setMinValue:0];
    [_speed setMaxValue:1];
    [_speed setDoubleValue:sliderValue(_settings.doubleClickInterval)];
    [_speed setContinuous:NO];
    [_speed setTarget:self];
    [_speed setAction:@selector(speedChanged:)];
    [view addSubview:_speed];
    [view addSubview:label([self text:@"Slow"], NSMakeRect(214, 46, 100, 16), 11, NSLeftTextAlignment)];
    [view addSubview:label([self text:@"Fast"], NSMakeRect(354, 46, 100, 16), 11, NSRightTextAlignment)];

    [self setMainView:view];
    return view;
}

- (void)apply:(SLSMouseSettings)s
{
    int err = SLSSetMouseSettings(&s);

    if (err == kSLSErrorSuccess) {
        _settings = s;
        CFPreferencesSetValue(CFSTR("com.apple.mouse.swapLeftRightButton"),
                              s.swapButtons ? kCFBooleanTrue : kCFBooleanFalse, kCFPreferencesAnyApplication,
                              kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        double seconds = s.doubleClickInterval / 1000.0;
        CFNumberRef threshold = CFNumberCreate(NULL, kCFNumberDoubleType, &seconds);
        CFPreferencesSetValue(CFSTR("com.apple.mouse.doubleClickThreshold"), threshold,
                              kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        CFRelease(threshold);
        CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    } else
        NSBeep();
    /* The radios show what is in force. */
    [_left setState:_settings.swapButtons ? NSControlStateValueOff : NSControlStateValueOn];
    [_right setState:_settings.swapButtons ? NSControlStateValueOn : NSControlStateValueOff];
    os_log_info(OS_LOG_DEFAULT, "mouse primary %{public}s double-click %u ms: %d",
                s.swapButtons ? "right" : "left", s.doubleClickInterval, err);
}

- (void)primaryChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.swapButtons = sender == _right ? 1 : 0;
    [self apply:s];
}

- (void)speedChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.doubleClickInterval = interval([_speed doubleValue]);
    [self apply:s];
}

@end
