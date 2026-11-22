/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Mouse pane, with the reference platform's settings: Scroll
 * direction: Natural, Tracking speed, Double-Click speed (150 ms to 2 s
 * between the clicks), Scrolling speed and the primary mouse button (left,
 * or right for the left-handed), set in the window server
 * (SLSSetMouseSettings), which counts the clicks, swaps the buttons,
 * inverts the wheel, gives IOHIDSystem the acceleration and keeps the
 * settings. The reference's global keys are written too, for apps that
 * read them (measured on it, 22 Nov 2026): com.apple.swipescrolldirection,
 * com.apple.mouse.scaling, com.apple.mouse.doubleClickThreshold,
 * com.apple.scrollwheel.scaling, com.apple.mouse.swapLeftRightButton.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <SkyLight/SkyLight.h>
#include <math.h>
#include <os/log.h>

@interface MousePane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 300;
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
    NSButton *_left, *_right, *_natural;
    NSSlider *_speed, *_tracking, *_scrolling;
    SLSMouseSettings _settings;
}

/* A titled slider, Slow to Fast, at x, y. */
- (NSSlider *)slider:(NSString *)title x:(CGFloat)x y:(CGFloat)y max:(double)max value:(double)value
              action:(SEL)action in:(NSView *)view
{
    [view addSubview:label([self text:title], NSMakeRect(x, y + 30, 240, 18), 13, NSCenterTextAlignment)];
    NSSlider *s = [[NSSlider alloc] initWithFrame:NSMakeRect(x, y, 240, 24)];
    [s setMinValue:0];
    [s setMaxValue:max];
    [s setDoubleValue:value];
    [s setContinuous:NO];
    [s setTarget:self];
    [s setAction:action];
    [view addSubview:s];
    [view addSubview:label([self text:@"Slow"], NSMakeRect(x, y - 20, 100, 16), 11, NSLeftTextAlignment)];
    [view addSubview:label([self text:@"Fast"], NSMakeRect(x + 140, y - 20, 100, 16), 11, NSRightTextAlignment)];
    return s;
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

    memset(&_settings, 0, sizeof(_settings));
    if (SLSGetMouseSettings(&_settings) != kSLSErrorSuccess) {
        _settings.swapButtons = 0;
        _settings.doubleClickInterval = 500;
        _settings.naturalScroll = 1;
    }

    /* Scroll direction: Natural. */
    _natural = [[NSButton alloc] initWithFrame:NSMakeRect(214, 256, 300, 20)];
    [_natural setButtonType:NSButtonTypeSwitch];
    [_natural setTitle:[self text:@"Scroll direction: Natural"]];
    [_natural setState:_settings.naturalScroll ? NSControlStateValueOn : NSControlStateValueOff];
    [_natural setTarget:self];
    [_natural setAction:@selector(naturalChanged:)];
    [view addSubview:_natural];

    /* Tracking speed and Double-Click speed; Scrolling speed. */
    _tracking = [self slider:@"Tracking speed" x:60 y:180 max:3
                       value:_settings.trackingSpeed ? _settings.trackingSpeed / 1000.0 : 1
                      action:@selector(trackingChanged:) in:view];
    _speed = [self slider:@"Double-Click speed" x:368 y:180 max:1
                    value:sliderValue(_settings.doubleClickInterval) action:@selector(speedChanged:) in:view];
    _scrolling = [self slider:@"Scrolling speed" x:60 y:90 max:3
                        value:_settings.scrollSpeed ? _settings.scrollSpeed / 1000.0 : 1
                       action:@selector(scrollingChanged:) in:view];

    /* Primary mouse button: Left, Right. */
    [view addSubview:label([self text:@"Primary mouse button:"], NSMakeRect(368, 122, 240, 20), 13,
                           NSCenterTextAlignment)];
    _left = [self radio:@"Left" frame:NSMakeRect(410, 96, 80, 20)];
    _right = [self radio:@"Right" frame:NSMakeRect(500, 96, 80, 20)];
    [_left setState:_settings.swapButtons ? NSControlStateValueOff : NSControlStateValueOn];
    [_right setState:_settings.swapButtons ? NSControlStateValueOn : NSControlStateValueOff];
    [view addSubview:_left];
    [view addSubview:_right];

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
        CFPreferencesSetValue(CFSTR("com.apple.swipescrolldirection"),
                              s.naturalScroll ? kCFBooleanTrue : kCFBooleanFalse, kCFPreferencesAnyApplication,
                              kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        struct { CFStringRef key; uint32_t value; } speeds[] = {
            { CFSTR("com.apple.mouse.scaling"), s.trackingSpeed },
            { CFSTR("com.apple.scrollwheel.scaling"), s.scrollSpeed } };
        for (size_t i = 0; i < 2; i++) {
            if (speeds[i].value == 0)
                continue;
            double scale = speeds[i].value / 1000.0;
            CFNumberRef n = CFNumberCreate(NULL, kCFNumberDoubleType, &scale);
            CFPreferencesSetValue(speeds[i].key, n, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser,
                                  kCFPreferencesAnyHost);
            CFRelease(n);
        }
        CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    } else
        NSBeep();
    /* The radios show what is in force. */
    [_left setState:_settings.swapButtons ? NSControlStateValueOff : NSControlStateValueOn];
    [_right setState:_settings.swapButtons ? NSControlStateValueOn : NSControlStateValueOff];
    [_natural setState:_settings.naturalScroll ? NSControlStateValueOn : NSControlStateValueOff];
    os_log_info(OS_LOG_DEFAULT, "mouse primary %{public}s double-click %u ms natural %u tracking %u scrolling %u: %d",
                s.swapButtons ? "right" : "left", s.doubleClickInterval, s.naturalScroll, s.trackingSpeed,
                s.scrollSpeed, err);
}

- (void)primaryChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.swapButtons = sender == _right ? 1 : 0;
    [self apply:s];
}

- (void)naturalChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.naturalScroll = [_natural state] == NSControlStateValueOn ? 1 : 0;
    [self apply:s];
}

- (void)trackingChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.trackingSpeed = (uint32_t)lround([_tracking doubleValue] * 1000);
    [self apply:s];
}

- (void)scrollingChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.scrollSpeed = (uint32_t)lround([_scrolling doubleValue] * 1000);
    [self apply:s];
}

- (void)speedChanged:(id)sender
{
    SLSMouseSettings s = _settings;
    s.doubleClickInterval = interval([_speed doubleValue]);
    [self apply:s];
}

@end
