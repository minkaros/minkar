/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Keyboard pane (ROADMAP Phase 5, item 6): Key Repeat and Delay Until
 * Repeat (the reference platform's ranges: 30 to 300 ms between repeats,
 * 225 to 1800 ms before the first), set in the window server
 * (SLSSetKeyboardSettings), which keeps them and applies them at once; the
 * reference's global keys are written too (KeyRepeat and InitialKeyRepeat,
 * in 15 ms ticks). The Input Sources tab, as the reference's: the enabled
 * keyboard layouts (Carbon's TIS API, kept in com.apple.HIToolbox), + and
 * -, and Show Input menu in menu bar (the TextInput Menu Extra, and
 * com.apple.TextInputMenu's visible); with more than one source the menu
 * shows, as on the reference. The Text tab, as the reference's
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
#include <Carbon/Carbon.h>
#include <notify.h>
#import "../MenuExtraList.h"
#include <SkyLight/SkyLight.h>
#include <os/log.h>

@interface KeyboardPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate>
@end

static const CGFloat kWidth = 668, kHeight = 320;
static const double kFastest = 30, kSlowest = 300, kShortest = 225, kLongest = 1800;


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
    SLSKeyboardSettings _settings;
    NSArray<NSButton *> *_textBoxes;
    NSTableView *_sources;
    NSArray *_enabled;          /* TISInputSourceRef, the enabled ones */
    NSPopUpButton *_add;
    NSButton *_remove, *_showMenu;
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

    /* The layout is chosen in Input Sources, as on the reference. */
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

    /* Input Sources: the enabled ones (Carbon's input source API), + and -,
       and the Input menu in the menu bar. */
    NSTabViewItem *inputTab = [[NSTabViewItem alloc] initWithIdentifier:@"input"];
    [inputTab setLabel:[self text:@"Input Sources"]];
    NSView *input = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 40, 260)];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 70, 260, 180)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _sources = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [column setWidth:256];
    [column setEditable:NO];
    [_sources addTableColumn:column];
    [_sources setHeaderView:nil];
    [_sources setDataSource:self];
    [_sources setDelegate:self];
    [scroll setDocumentView:_sources];
    [input addSubview:scroll];
    _add = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(20, 42, 40, 24) pullsDown:YES];
    [_add setTarget:self];
    [_add setAction:@selector(addSource:)];
    [input addSubview:_add];
    _remove = [[NSButton alloc] initWithFrame:NSMakeRect(62, 42, 28, 24)];
    [_remove setTitle:@"\u2212"];
    [_remove setBezelStyle:NSSmallSquareBezelStyle];
    [_remove setTarget:self];
    [_remove setAction:@selector(removeSource:)];
    [input addSubview:_remove];
    _showMenu = [[NSButton alloc] initWithFrame:NSMakeRect(300, 228, 300, 20)];
    [_showMenu setButtonType:NSButtonTypeSwitch];
    [_showMenu setTitle:[self text:@"Show Input menu in menu bar"]];
    [_showMenu setState:MenuExtraIsShown(@"TextInput") ? NSOnState : NSOffState];
    [_showMenu setTarget:self];
    [_showMenu setAction:@selector(showMenuChanged:)];
    [input addSubview:_showMenu];
    [inputTab setView:input];
    [tabs addTabViewItem:inputTab];
    [self reloadSources];

    [root addSubview:tabs];
    [self setMainView:root];
    return root;
}

/* ---- Input Sources --------------------------------------------------------- */

- (void)reloadSources
{
    _enabled = CFBridgingRelease(TISCreateInputSourceList(NULL, false));
    [_sources reloadData];
    [_add removeAllItems];
    [_add addItemWithTitle:@"+"];
    NSArray *all = CFBridgingRelease(TISCreateInputSourceList(NULL, true));
    for (id source in all) {
        TISInputSourceRef s = (__bridge TISInputSourceRef)source;
        if (CFBooleanGetValue(TISGetInputSourceProperty(s, kTISPropertyInputSourceIsEnabled)))
            continue;
        [_add addItemWithTitle:(__bridge NSString *)TISGetInputSourceProperty(s, kTISPropertyLocalizedName)];
        [[_add lastItem] setRepresentedObject:source];
    }
    [_add setEnabled:[_add numberOfItems] > 1];
    [self updateRemove];
}

- (void)updateRemove
{
    NSInteger row = [_sources selectedRow];
    BOOL selected = row >= 0 && (NSUInteger)row < [_enabled count] &&
        CFBooleanGetValue(TISGetInputSourceProperty((__bridge TISInputSourceRef)_enabled[(NSUInteger)row],
                                                    kTISPropertyInputSourceIsSelected));
    /* Not the one in use, nor the last. */
    [_remove setEnabled:row >= 0 && !selected && [_enabled count] > 1];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_enabled count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    return (__bridge NSString *)TISGetInputSourceProperty((__bridge TISInputSourceRef)_enabled[(NSUInteger)row],
                                                          kTISPropertyLocalizedName);
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [self updateRemove];
}

- (void)setInputMenuShown:(BOOL)shown
{
    MenuExtraSetShown(@"TextInput", shown);
    /* The reference's key for the Input menu. */
    CFPreferencesSetAppValue(CFSTR("visible"), shown ? kCFBooleanTrue : kCFBooleanFalse, CFSTR("com.apple.TextInputMenu"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.TextInputMenu"));
    [_showMenu setState:shown ? NSOnState : NSOffState];
}

- (void)addSource:(NSPopUpButton *)sender
{
    id source = [[sender selectedItem] representedObject];
    if (source == nil)
        return;
    OSStatus err = TISEnableInputSource((__bridge TISInputSourceRef)source);
    os_log_info(OS_LOG_DEFAULT, "keyboard input source %{public}s enabled: %d",
                [(__bridge NSString *)TISGetInputSourceProperty((__bridge TISInputSourceRef)source,
                    kTISPropertyInputSourceID) UTF8String], (int)err);
    [self reloadSources];
    /* As on the reference: with more than one, the Input menu shows. */
    if ([_enabled count] > 1 && !MenuExtraIsShown(@"TextInput"))
        [self setInputMenuShown:YES];
}

- (void)removeSource:(id)sender
{
    NSInteger row = [_sources selectedRow];
    if (row < 0 || (NSUInteger)row >= [_enabled count])
        return;
    OSStatus err = TISDisableInputSource((__bridge TISInputSourceRef)_enabled[(NSUInteger)row]);
    os_log_info(OS_LOG_DEFAULT, "keyboard input source removed: %d", (int)err);
    [self reloadSources];
}

- (void)showMenuChanged:(NSButton *)sender
{
    [self setInputMenuShown:[sender state] == NSOnState];
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

    /* The layout in use stays (another is selected through Input Sources). */
    SLSGetKeyboardSettings(&s);
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
