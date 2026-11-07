/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Keyboard pane (ROADMAP Phase 5, item 6): Key Repeat and Delay Until
 * Repeat (the reference platform's ranges: 30 to 300 ms between repeats,
 * 225 to 1800 ms before the first) and the keyboard layout, set in the
 * window server (SLSSetKeyboardSettings), which keeps them and applies
 * them at once. Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <SkyLight/SkyLight.h>
#include <os/log.h>

@interface KeyboardPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 230;
static const double kFastest = 30, kSlowest = 300, kShortest = 225, kLongest = 1800;

/* The layouts offered: XKB layout, variant, English name. */
static const struct { const char *layout, *variant, *name; } kLayouts[] = {
	{ "us", "", "English (US)" },
	{ "gb", "", "English (UK)" },
	{ "es", "", "Spanish" },
	{ "latam", "", "Spanish (Latin American)" },
	{ "br", "", "Portuguese (Brazil)" },
	{ "fr", "", "French" },
	{ "de", "", "German" },
	{ "it", "", "Italian" },
};
#define kLayoutCount (sizeof(kLayouts) / sizeof(kLayouts[0]))

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

@implementation KeyboardPane {
    NSSlider *_repeat, *_delay;
    NSPopUpButton *_layout;
    SLSKeyboardSettings _settings;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

static NSSlider *slider(NSRect frame, double value, id target)
{
    NSSlider *s = [[NSSlider alloc] initWithFrame:frame];
    [s setMinValue:0];
    [s setMaxValue:1];
    [s setDoubleValue:value];
    [s setContinuous:NO];
    [s setTarget:target];
    [s setAction:@selector(changed:)];
    return s;
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    if (SLSGetKeyboardSettings(&_settings) != kSLSErrorSuccess) {
        memset(&_settings, 0, sizeof(_settings));
        strlcpy(_settings.layout, "us", sizeof(_settings.layout));
        _settings.initialRepeat = 500;
        _settings.repeatInterval = 33;
    }

    /* Key Repeat: Slow to Fast; Delay Until Repeat: Long to Short. */
    [view addSubview:label([self text:@"Key Repeat"], NSMakeRect(60, 190, 240, 18), 13, NSCenterTextAlignment)];
    _repeat = slider(NSMakeRect(60, 160, 240, 24),
                     (kSlowest - _settings.repeatInterval) / (kSlowest - kFastest), self);
    [view addSubview:_repeat];
    [view addSubview:label([self text:@"Slow"], NSMakeRect(60, 140, 100, 16), 11, NSLeftTextAlignment)];
    [view addSubview:label([self text:@"Fast"], NSMakeRect(200, 140, 100, 16), 11, NSRightTextAlignment)];

    [view addSubview:label([self text:@"Delay Until Repeat"], NSMakeRect(368, 190, 240, 18), 13, NSCenterTextAlignment)];
    _delay = slider(NSMakeRect(368, 160, 240, 24),
                    (kLongest - _settings.initialRepeat) / (kLongest - kShortest), self);
    [view addSubview:_delay];
    [view addSubview:label([self text:@"Long"], NSMakeRect(368, 140, 100, 16), 11, NSLeftTextAlignment)];
    [view addSubview:label([self text:@"Short"], NSMakeRect(508, 140, 100, 16), 11, NSRightTextAlignment)];

    [view addSubview:label([self text:@"Keyboard layout:"], NSMakeRect(60, 74, 170, 20), 13, NSRightTextAlignment)];
    _layout = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(240, 70, 260, 26) pullsDown:NO];
    NSInteger selected = 0;
    for (size_t i = 0; i < kLayoutCount; i++) {
        [_layout addItemWithTitle:[self text:@(kLayouts[i].name)]];
        if (strcmp(kLayouts[i].layout, _settings.layout) == 0 && strcmp(kLayouts[i].variant, _settings.variant) == 0)
            selected = (NSInteger)i;
    }
    [_layout selectItemAtIndex:selected];
    [_layout setTarget:self];
    [_layout setAction:@selector(changed:)];
    [view addSubview:_layout];

    [self setMainView:view];
    return view;
}

- (void)changed:(id)sender
{
    SLSKeyboardSettings s = _settings;
    NSInteger i = [_layout indexOfSelectedItem];

    if (i >= 0 && (size_t)i < kLayoutCount) {
        strlcpy(s.layout, kLayouts[i].layout, sizeof(s.layout));
        strlcpy(s.variant, kLayouts[i].variant, sizeof(s.variant));
    }
    s.repeatInterval = (uint32_t)lround(kSlowest - [_repeat doubleValue] * (kSlowest - kFastest));
    s.initialRepeat = (uint32_t)lround(kLongest - [_delay doubleValue] * (kLongest - kShortest));
    int err = SLSSetKeyboardSettings(&s);
    if (err == kSLSErrorSuccess)
        _settings = s;
    else
        NSBeep();
    os_log_info(OS_LOG_DEFAULT, "keyboard %{public}s repeat %u delay %u: %d", s.layout,
                s.repeatInterval, s.initialRepeat, err);
}

@end
