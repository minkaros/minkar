/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Keyboard pane's Shortcuts tab, as the reference's: the categories on
 * the left, their shortcuts on the right, each with its box and its keys;
 * a double-click records new keys (Escape cancels), Restore Defaults puts
 * the category's back. Only the shortcuts Atlas has are listed, with the
 * reference's numbers and default keys. They are kept in the user's
 * com.apple.symbolichotkeys domain ("AppleSymbolicHotKeys": number ->
 * { enabled; value = { parameters = (character, key code, modifier
 * flags); type = standard } }) and given to the window server at once
 * (SLSSetSymbolicHotKey); SystemUIServer gives them again at login.
 */

#import "KeyboardShortcuts.h"
#include <SkyLight/SkyLight.h>
#include <os/log.h>

static CFStringRef const kDomain = CFSTR("com.apple.symbolichotkeys");
static NSString *const kTableKey = @"AppleSymbolicHotKeys";

/* The modifier flags kept: Shift, Control, Option, Command, the keypad's
   and the function keys' (the server compares the first four). */
static const NSUInteger kKeptModifiers = 0x00FE0000;

typedef struct {
    uint32_t number;
    const char *title;
    uint16_t character, keyCode;	/* character 65535: none */
    uint32_t modifiers;
} Shortcut;

typedef struct {
    const char *title;
    Shortcut shortcuts[4];		/* number 0 ends */
} ShortcutCategory;

/* The reference's defaults (measured on it, 22 Nov 2026). */
static const ShortcutCategory kCategories[] = {
    { "Dock", {
        { kSLSHotKeyDockHiding, "Turn Dock Hiding On/Off", 'd', 2, 0x180000 },
    } },
    { "Keyboard", {
        { kSLSHotKeyMenuBarFocus, "Move focus to the menu bar", 65535, 120, 0x840000 },
        { kSLSHotKeyNextWindow, "Move focus to next window", '`', 50, 0x100000 },
    } },
    { "Input Sources", {
        { kSLSHotKeyPreviousInputSource, "Select the previous input source", ' ', 49, 0x040000 },
        { kSLSHotKeyNextInputSource, "Select next source in Input menu", ' ', 49, 0x0C0000 },
    } },
    { "Screenshots", {
        { kSLSHotKeyScreenshot, "Save picture of screen as a file", '3', 20, 0x120000 },
        { kSLSHotKeyScreenshotToClipboard, "Copy picture of screen to the clipboard", '3', 20, 0x160000 },
    } },
};
#define kCategoryCount (sizeof(kCategories) / sizeof(kCategories[0]))

static NSUInteger shortcutCount(const ShortcutCategory *category)
{
    NSUInteger n = 0;
    while (n < 4 && category->shortcuts[n].number != 0)
        n++;
    return n;
}

/* A key with no character, by its key code. */
static NSString *keyName(uint16_t keyCode)
{
    static const struct { uint16_t code; const char *name; } names[] = {
        { 122, "F1" }, { 120, "F2" }, { 99, "F3" }, { 118, "F4" }, { 96, "F5" }, { 97, "F6" },
        { 98, "F7" }, { 100, "F8" }, { 101, "F9" }, { 109, "F10" }, { 103, "F11" }, { 111, "F12" },
        { 123, "←" }, { 124, "→" }, { 125, "↓" }, { 126, "↑" },
        { 49, "Space" }, { 48, "⇥" }, { 53, "⎋" }, { 36, "↩" }, { 51, "⌫" },
        { 117, "⌦" }, { 115, "↖" }, { 119, "↘" }, { 116, "⇞" }, { 121, "⇟" },
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (names[i].code == keyCode)
            return [NSString stringWithUTF8String:names[i].name];
    return [NSString stringWithFormat:@"#%u", keyCode];
}

@implementation KeyboardShortcuts {
    NSBundle *_bundle;
    NSTableView *_categories, *_shortcuts;
    NSInteger _recording;		/* the row taking new keys, or -1 */
    id _monitor;
}

- (instancetype)initWithBundle:(NSBundle *)bundle
{
    if ((self = [super init]) != nil) {
        _bundle = bundle;
        _recording = -1;
    }
    return self;
}

- (void)dealloc
{
    if (_monitor != nil)
        [NSEvent removeMonitor:_monitor];
}

- (NSString *)text:(NSString *)english
{
    return [_bundle localizedStringForKey:english value:english table:nil];
}

static NSTableView *table(NSScrollView *scroll, id owner)
{
    NSTableView *t = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    [t setHeaderView:nil];
    [t setDataSource:owner];
    [t setDelegate:owner];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    [scroll setDocumentView:t];
    return t;
}

- (NSView *)viewWithFrame:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    NSTextField *hint = [[NSTextField alloc] initWithFrame:NSMakeRect(20, frame.size.height - 30, frame.size.width - 40, 18)];
    [hint setStringValue:[self text:@"To change a shortcut, double-click it and type the new keys."]];
    [hint setEditable:NO];
    [hint setSelectable:NO];
    [hint setBordered:NO];
    [hint setDrawsBackground:NO];
    [hint setFont:[NSFont systemFontOfSize:12]];
    [view addSubview:hint];

    CGFloat height = frame.size.height - 90;
    NSScrollView *left = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 50, 180, height)];
    _categories = table(left, self);
    NSTableColumn *name = [[NSTableColumn alloc] initWithIdentifier:@"category"];
    [name setWidth:176];
    [name setEditable:NO];
    [_categories addTableColumn:name];
    [view addSubview:left];

    NSScrollView *right = [[NSScrollView alloc] initWithFrame:NSMakeRect(210, 50, frame.size.width - 230, height)];
    _shortcuts = table(right, self);
    NSTableColumn *on = [[NSTableColumn alloc] initWithIdentifier:@"on"];
    NSButtonCell *box = [[NSButtonCell alloc] init];
    [box setButtonType:NSButtonTypeSwitch];
    [box setTitle:@""];
    [on setDataCell:box];
    [on setWidth:22];
    [_shortcuts addTableColumn:on];
    NSTableColumn *title = [[NSTableColumn alloc] initWithIdentifier:@"title"];
    [title setWidth:frame.size.width - 230 - 22 - 110 - 8];
    [title setEditable:NO];
    [_shortcuts addTableColumn:title];
    NSTableColumn *keys = [[NSTableColumn alloc] initWithIdentifier:@"keys"];
    [keys setWidth:106];
    [keys setEditable:NO];
    [_shortcuts addTableColumn:keys];
    [_shortcuts setTarget:self];
    [_shortcuts setDoubleAction:@selector(record:)];
    [view addSubview:right];

    NSButton *restore = [[NSButton alloc] initWithFrame:NSMakeRect(frame.size.width - 160, 12, 140, 28)];
    [restore setTitle:[self text:@"Restore Defaults"]];
    [restore setBezelStyle:NSRoundedBezelStyle];
    [restore setTarget:self];
    [restore setAction:@selector(restoreDefaults:)];
    [view addSubview:restore];

    [_categories reloadData];
    [_categories selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [_shortcuts reloadData];
    return view;
}

/* ---- What is kept ---------------------------------------------------------- */

- (const ShortcutCategory *)category
{
    NSInteger row = [_categories selectedRow];
    return &kCategories[row >= 0 && (NSUInteger)row < kCategoryCount ? row : 0];
}

static NSDictionary *userTable(void)
{
    CFPreferencesAppSynchronize(kDomain);
    NSDictionary *t = CFBridgingRelease(CFPreferencesCopyAppValue((__bridge CFStringRef)kTableKey, kDomain));
    return [t isKindOfClass:[NSDictionary class]] ? t : @{};
}

/* The shortcut as the user has it: enabled, character, key code, flags. */
static void current(const Shortcut *s, BOOL *enabled, uint16_t *character, uint16_t *keyCode, uint32_t *modifiers)
{
    NSDictionary *entry = userTable()[[NSString stringWithFormat:@"%u", s->number]];

    *enabled = YES;
    *character = s->character;
    *keyCode = s->keyCode;
    *modifiers = s->modifiers;
    if (![entry isKindOfClass:[NSDictionary class]])
        return;
    if ([entry[@"enabled"] respondsToSelector:@selector(boolValue)])
        *enabled = [entry[@"enabled"] boolValue];
    NSDictionary *value = entry[@"value"];
    NSArray *parameters = [value isKindOfClass:[NSDictionary class]] ? value[@"parameters"] : nil;
    if ([parameters isKindOfClass:[NSArray class]] && [parameters count] >= 3 &&
        [parameters[1] unsignedIntValue] != 0xffff) {
        *character = (uint16_t)[parameters[0] unsignedIntValue];
        *keyCode = (uint16_t)[parameters[1] unsignedIntValue];
        *modifiers = [parameters[2] unsignedIntValue];
    }
}

/* Kept for the user (nil: the default again) and given to the server. */
static void keep(const Shortcut *s, BOOL enabled, uint16_t character, uint16_t keyCode, uint32_t modifiers, BOOL isDefault)
{
    NSMutableDictionary *t = [userTable() mutableCopy];
    NSString *number = [NSString stringWithFormat:@"%u", s->number];

    if (isDefault)
        [t removeObjectForKey:number];
    else
        t[number] = @{ @"enabled": @(enabled),
                       @"value": @{ @"parameters": @[ @(character), @(keyCode), @(modifiers) ],
                                    @"type": @"standard" } };
    CFPreferencesSetAppValue((__bridge CFStringRef)kTableKey,
                             [t count] > 0 ? (__bridge CFPropertyListRef)t : NULL, kDomain);
    CFPreferencesAppSynchronize(kDomain);
    int err = SLSSetSymbolicHotKey(s->number, keyCode, modifiers, enabled);
    os_log_info(OS_LOG_DEFAULT, "shortcut %u: %d", s->number, err);
}

static NSString *keysText(uint16_t character, uint16_t keyCode, uint32_t modifiers)
{
    NSMutableString *text = [NSMutableString string];

    if (modifiers & NSEventModifierFlagControl)
        [text appendString:@"⌃"];
    if (modifiers & NSEventModifierFlagOption)
        [text appendString:@"⌥"];
    if (modifiers & NSEventModifierFlagShift)
        [text appendString:@"⇧"];
    if (modifiers & NSEventModifierFlagCommand)
        [text appendString:@"⌘"];
    if (character != 65535 && character > ' ')
        [text appendString:[[NSString stringWithFormat:@"%C", (unichar)character] uppercaseString]];
    else
        [text appendString:keyName(keyCode)];
    return text;
}

/* ---- The tables ---------------------------------------------------------- */

- (NSInteger)numberOfRowsInTableView:(NSTableView *)t
{
    return t == _categories ? (NSInteger)kCategoryCount : (NSInteger)shortcutCount([self category]);
}

- (id)tableView:(NSTableView *)t objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if (t == _categories)
        return [self text:[NSString stringWithUTF8String:kCategories[row].title]];
    const Shortcut *s = &[self category]->shortcuts[row];
    BOOL enabled;
    uint16_t character, keyCode;
    uint32_t modifiers;
    current(s, &enabled, &character, &keyCode, &modifiers);
    if ([[column identifier] isEqual:@"on"])
        return @(enabled);
    if ([[column identifier] isEqual:@"title"])
        return [self text:[NSString stringWithUTF8String:s->title]];
    if (row == _recording)
        return [self text:@"Type the keys"];
    return keysText(character, keyCode, modifiers);
}

- (void)tableView:(NSTableView *)t setObjectValue:(id)value forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    if (t != _shortcuts || ![[column identifier] isEqual:@"on"])
        return;
    const Shortcut *s = &[self category]->shortcuts[row];
    BOOL enabled;
    uint16_t character, keyCode;
    uint32_t modifiers;
    current(s, &enabled, &character, &keyCode, &modifiers);
    enabled = [value boolValue];
    keep(s, enabled, character, keyCode, modifiers,
         enabled && character == s->character && keyCode == s->keyCode && modifiers == s->modifiers);
    [_shortcuts reloadData];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    if ([note object] == _categories) {
        [self stopRecording];
        [_shortcuts deselectAll:nil];
        [_shortcuts reloadData];
    }
}

/* ---- Recording ----------------------------------------------------------- */

- (void)stopRecording
{
    _recording = -1;
    if (_monitor != nil) {
        [NSEvent removeMonitor:_monitor];
        _monitor = nil;
    }
}

- (void)record:(id)sender
{
    NSInteger row = [_shortcuts clickedRow];
    if (row < 0 || [_shortcuts clickedColumn] == 0)
        return;
    [self stopRecording];
    _recording = row;
    __weak KeyboardShortcuts *weakSelf = self;
    _monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown | NSEventMaskLeftMouseDown
                                                     handler:^NSEvent *(NSEvent *event) {
        KeyboardShortcuts *me = weakSelf;
        return me != nil ? [me recorded:event] : event;
    }];
    [_shortcuts reloadData];
}

/* The keys typed while recording: kept, unless Escape alone (cancel) or a
   key with no modifier that is not a function key. A click elsewhere
   cancels. */
- (NSEvent *)recorded:(NSEvent *)event
{
    if (_recording < 0)
        return event;
    if ([event type] != NSEventTypeKeyDown) {
        [self stopRecording];
        [_shortcuts reloadData];
        return event;
    }
    NSUInteger modifiers = [event modifierFlags] & kKeptModifiers;
    uint16_t keyCode = [event keyCode];
    BOOL functionKey = keyName(keyCode).length > 1 && [keyName(keyCode) hasPrefix:@"F"];
    NSInteger row = _recording;
    if (keyCode == 53 && (modifiers & 0x1E0000) == 0) {
        [self stopRecording];
        [_shortcuts reloadData];
        return nil;
    }
    if ((modifiers & 0x1E0000) == 0 && !functionKey) {
        NSBeep();
        return nil;
    }
    /* The key's own character, with no Shift (as the reference keeps
       Shift-Command-3: 51, "3"). */
    NSString *characters = [[event charactersByApplyingModifiers:0] lowercaseString];
    uint16_t character = 65535;
    if ([characters length] == 1 && [characters characterAtIndex:0] >= ' ' && [characters characterAtIndex:0] < 0x7f)
        character = [characters characterAtIndex:0];
    const Shortcut *s = &[self category]->shortcuts[row];
    BOOL enabled, isDefault = keyCode == s->keyCode && (uint32_t)modifiers == (s->modifiers & kKeptModifiers);
    uint16_t oldCharacter, oldKeyCode;
    uint32_t oldModifiers;
    current(s, &enabled, &oldCharacter, &oldKeyCode, &oldModifiers);
    if (isDefault) {
        character = s->character;
        modifiers = s->modifiers;
    }
    keep(s, enabled, character, keyCode, (uint32_t)modifiers, isDefault && enabled);
    [self stopRecording];
    [_shortcuts reloadData];
    return nil;
}

- (void)restoreDefaults:(id)sender
{
    const ShortcutCategory *category = [self category];

    [self stopRecording];
    for (NSUInteger i = 0; i < shortcutCount(category); i++) {
        const Shortcut *s = &category->shortcuts[i];
        keep(s, YES, s->character, s->keyCode, s->modifiers, YES);
    }
    [_shortcuts reloadData];
}

@end
