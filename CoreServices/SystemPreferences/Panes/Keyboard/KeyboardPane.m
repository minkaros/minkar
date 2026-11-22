/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Keyboard pane (ROADMAP Phase 5, item 6): Key Repeat and Delay Until
 * Repeat (the reference platform's ranges: 30 to 300 ms between repeats,
 * 225 to 1800 ms before the first) and the keyboard layout, set in the
 * window server (SLSSetKeyboardSettings), which keeps them and applies
 * them at once; the reference's global keys are written too (KeyRepeat
 * and InitialKeyRepeat, in 15 ms ticks). The Text tab, as the reference's
 * (its keys measured on it, 22 Nov 2026): Correct spelling automatically
 * (NSAutomaticSpellingCorrectionEnabled, WebAutomaticSpellingCorrection-
 * Enabled), Capitalize words automatically (NSAutomaticCapitalization-
 * Enabled), Add period with double-space (NSAutomaticPeriodSubstitution-
 * Enabled) and Use smart quotes and dashes (NSAutomaticQuoteSubstitution-
 * Enabled, NSAutomaticDashSubstitutionEnabled), all on when unset; AppKit's
 * text views follow them.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <SkyLight/SkyLight.h>
#include <os/log.h>

@interface KeyboardPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 320;
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
    NSArray<NSButton *> *_textBoxes;
}

/* The Text tab's boxes: their titles and the global keys each sets. */
static NSArray *textSettings(void)
{
    return @[ @[ @"Correct spelling automatically", @[ @"NSAutomaticSpellingCorrectionEnabled",
                                                       @"WebAutomaticSpellingCorrectionEnabled" ] ],
              @[ @"Capitalize words automatically", @[ @"NSAutomaticCapitalizationEnabled" ] ],
              @[ @"Add period with double-space", @[ @"NSAutomaticPeriodSubstitutionEnabled" ] ],
              @[ @"Use smart quotes and dashes", @[ @"NSAutomaticQuoteSubstitutionEnabled",
                                                    @"NSAutomaticDashSubstitutionEnabled" ] ] ];
}

static BOOL globalOn(NSString *key)
{
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    Boolean valid = false;
    Boolean on = CFPreferencesGetAppBooleanValue((__bridge CFStringRef)key, kCFPreferencesAnyApplication, &valid);
    return valid ? on : YES;
}

static void setGlobal(NSString *key, CFPropertyListRef value)
{
    CFPreferencesSetValue((__bridge CFStringRef)key, value, kCFPreferencesAnyApplication,
                          kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
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
    NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(20, 10, kWidth - 40, kHeight - 20)];
    NSTabViewItem *keyboardTab = [[NSTabViewItem alloc] initWithIdentifier:@"keyboard"];
    [keyboardTab setLabel:[self text:@"Keyboard"]];
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 40, 260)];

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
    [keyboardTab setView:view];
    [tabs addTabViewItem:keyboardTab];

    /* Text. */
    NSTabViewItem *textTab = [[NSTabViewItem alloc] initWithIdentifier:@"text"];
    [textTab setLabel:[self text:@"Text"]];
    NSView *text = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 40, 260)];
    NSMutableArray *boxes = [NSMutableArray array];
    CGFloat y = 200;
    for (NSArray *setting in textSettings()) {
        NSButton *box = [[NSButton alloc] initWithFrame:NSMakeRect(40, y, 400, 20)];
        [box setButtonType:NSButtonTypeSwitch];
        [box setTitle:[self text:setting[0]]];
        [box setState:globalOn(setting[1][0]) ? NSOnState : NSOffState];
        [box setTarget:self];
        [box setAction:@selector(textChanged:)];
        [text addSubview:box];
        [boxes addObject:box];
        y -= 30;
    }
    _textBoxes = boxes;
    [textTab setView:text];
    [tabs addTabViewItem:textTab];

    [root addSubview:tabs];
    [self setMainView:root];
    return root;
}

- (void)textChanged:(NSButton *)sender
{
    NSUInteger i = [_textBoxes indexOfObject:sender];
    if (i == NSNotFound)
        return;
    int on = [sender state] == NSOnState;
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &on);
    for (NSString *key in textSettings()[i][1])
        setGlobal(key, n);
    CFRelease(n);
    os_log_info(OS_LOG_DEFAULT, "keyboard text %{public}s %d", [textSettings()[i][0] UTF8String], on);
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
    if (err == kSLSErrorSuccess) {
        _settings = s;
        /* The reference's keys, in 15 ms ticks. */
        int ticks = (int)lround(s.repeatInterval / 15.0), initial = (int)lround(s.initialRepeat / 15.0);
        CFNumberRef r = CFNumberCreate(NULL, kCFNumberIntType, &ticks);
        CFNumberRef d = CFNumberCreate(NULL, kCFNumberIntType, &initial);
        setGlobal(@"KeyRepeat", r);
        setGlobal(@"InitialKeyRepeat", d);
        CFRelease(r);
        CFRelease(d);
    } else
        NSBeep();
    os_log_info(OS_LOG_DEFAULT, "keyboard %{public}s repeat %u delay %u: %d", s.layout,
                s.repeatInterval, s.initialRepeat, err);
}

@end
