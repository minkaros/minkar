/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMPreferencesController.h"
#import "TMPreferences.h"
#include <pwd.h>
#include <unistd.h>

#define WIDTH 580.0
#define HEIGHT 380.0
#define LABEL_WIDTH 170.0

@implementation TMPreferencesController

+ (TMPreferencesController *)sharedController
{
    static TMPreferencesController *shared;

    if (shared == nil)
        shared = [[TMPreferencesController alloc] init];
    return shared;
}

/* ---- Building the window ---- */

static NSTextField *label(NSView *view, NSString *text, NSRect frame, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];

    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    [view addSubview:field];
    return field;
}

/* A row: its label on the left, right-aligned; returns where the control starts. */
static CGFloat row(NSView *view, NSString *text, CGFloat y)
{
    label(view, text, NSMakeRect(10, y + 1, LABEL_WIDTH, 18), NSRightTextAlignment);
    return LABEL_WIDTH + 18;
}

- (NSTextField *)field:(NSView *)view frame:(NSRect)frame
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];

    [[field cell] setSendsActionOnEndEditing:YES];
    [field setTarget:self];
    [field setAction:@selector(takeValue:)];
    [view addSubview:field];
    return field;
}

- (NSPopUpButton *)popUp:(NSView *)view frame:(NSRect)frame titles:(NSArray<NSString *> *)titles
{
    NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO];

    [button addItemsWithTitles:titles];
    [button setTarget:self];
    [button setAction:@selector(takeValue:)];
    [view addSubview:button];
    return button;
}

- (NSColorWell *)well:(NSView *)view frame:(NSRect)frame
{
    NSColorWell *well = [[NSColorWell alloc] initWithFrame:frame];

    [well setTarget:self];
    [well setAction:@selector(takeColor:)];
    [view addSubview:well];
    return well;
}

- (NSView *)generalView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    CGFloat top = NSHeight(frame) - 40, x;
    struct passwd *pw = getpwuid(getuid());
    NSString *own = pw && pw->pw_shell ? @(pw->pw_shell) : @"/bin/sh";

    x = row(view, NSLocalizedString(@"Shell:", nil), top);
    _shellField = [self field:view frame:NSMakeRect(x, top, 250, 22)];
    [[_shellField cell] setPlaceholderString:[NSString stringWithFormat:NSLocalizedString(@"Login shell (%@)", nil), own]];

    x = row(view, NSLocalizedString(@"When the shell exits:", nil), top - 36);
    _exitPopUp = [self popUp:view frame:NSMakeRect(x, top - 38, 290, 26) titles:@[
        NSLocalizedString(@"Close the tab", nil),
        NSLocalizedString(@"Close if it exited cleanly", nil),
        NSLocalizedString(@"Don't close", nil) ]];

    x = row(view, NSLocalizedString(@"Window size:", nil), top - 80);
    _columnsField = [self field:view frame:NSMakeRect(x, top - 80, 56, 22)];
    label(view, NSLocalizedString(@"columns", nil), NSMakeRect(x + 62, top - 79, 70, 18), NSLeftTextAlignment);
    _rowsField = [self field:view frame:NSMakeRect(x + 134, top - 80, 56, 22)];
    label(view, NSLocalizedString(@"rows", nil), NSMakeRect(x + 196, top - 79, 60, 18), NSLeftTextAlignment);

    x = row(view, NSLocalizedString(@"Scrollback:", nil), top - 116);
    _scrollbackField = [self field:view frame:NSMakeRect(x, top - 116, 80, 22)];
    label(view, NSLocalizedString(@"lines", nil), NSMakeRect(x + 86, top - 115, 80, 18), NSLeftTextAlignment);

    x = row(view, NSLocalizedString(@"Bell:", nil), top - 156);
    _bellPopUp = [self popUp:view frame:NSMakeRect(x, top - 158, 200, 26) titles:@[
        NSLocalizedString(@"Play a sound", nil),
        NSLocalizedString(@"Flash the screen", nil),
        NSLocalizedString(@"Nothing", nil) ]];
    return view;
}

- (NSView *)textView:(NSRect)frame
{
    NSView *view = [[NSView alloc] initWithFrame:frame];
    CGFloat top = NSHeight(frame) - 40, x;
    NSMutableArray *schemes = [NSMutableArray array];
    NSMutableArray *wells = [NSMutableArray array];

    x = row(view, NSLocalizedString(@"Text size:", nil), top);
    _fontSizeField = [self field:view frame:NSMakeRect(x, top, 50, 22)];
    label(view, NSLocalizedString(@"points (Agave)", nil), NSMakeRect(x + 56, top + 1, 150, 18), NSLeftTextAlignment);

    for (NSString *name in [TMPreferences schemeNames])
        [schemes addObject:NSLocalizedString(name, nil)];
    [schemes addObject:NSLocalizedString(@"Custom", nil)];
    x = row(view, NSLocalizedString(@"Scheme:", nil), top - 36);
    _schemePopUp = [self popUp:view frame:NSMakeRect(x, top - 38, 160, 26) titles:schemes];
    [_schemePopUp setAction:@selector(takeScheme:)];

    x = row(view, NSLocalizedString(@"Background:", nil), top - 76);
    _backgroundWell = [self well:view frame:NSMakeRect(x, top - 78, 44, 26)];
    label(view, NSLocalizedString(@"Text:", nil), NSMakeRect(x + 52, top - 75, 50, 18), NSRightTextAlignment);
    _textWell = [self well:view frame:NSMakeRect(x + 108, top - 78, 44, 26)];

    x = row(view, NSLocalizedString(@"Cursor:", nil), top - 112);
    _cursorWell = [self well:view frame:NSMakeRect(x, top - 114, 44, 26)];
    _accentCursorButton = [[NSButton alloc] initWithFrame:NSMakeRect(x + 52, top - 112, 200, 22)];
    [_accentCursorButton setButtonType:NSSwitchButton];
    [_accentCursorButton setTitle:NSLocalizedString(@"Use the accent color", nil)];
    [_accentCursorButton setTarget:self];
    [_accentCursorButton setAction:@selector(takeValue:)];
    [view addSubview:_accentCursorButton];

    /* The sixteen ANSI colors: normal above, bright below. */
    x = row(view, NSLocalizedString(@"ANSI colors:", nil), top - 150);
    for (int i = 0; i < 16; i++) {
        NSColorWell *well = [self well:view frame:NSMakeRect(x + (i % 8) * 34, top - 152 - (i / 8) * 30, 30, 24)];
        [well setTag:i];
        [wells addObject:well];
    }
    label(view, NSLocalizedString(@"Normal", nil), NSMakeRect(x + 8 * 34 + 4, top - 151, 80, 18), NSLeftTextAlignment);
    label(view, NSLocalizedString(@"Bright", nil), NSMakeRect(x + 8 * 34 + 4, top - 181, 80, 18), NSLeftTextAlignment);
    _paletteWells = wells;
    return view;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, WIDTH, HEIGHT)
        styleMask:NSTitledWindowMask | NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];

    if ((self = [super initWithWindow:window]) == nil)
        return nil;
    [window setTitle:NSLocalizedString(@"Preferences", nil)];
    [window setReleasedWhenClosed:NO];

    NSView *content = [window contentView];
    _tabView = [[NSTabView alloc] initWithFrame:NSMakeRect(12, 50, WIDTH - 24, HEIGHT - 62)];
    NSRect inside = [_tabView contentRect];
    inside.origin = NSZeroPoint;
    NSTabViewItem *general = [[NSTabViewItem alloc] initWithIdentifier:@"General"];
    [general setLabel:NSLocalizedString(@"General", nil)];
    [general setView:[self generalView:inside]];
    NSTabViewItem *text = [[NSTabViewItem alloc] initWithIdentifier:@"Text"];
    [text setLabel:NSLocalizedString(@"Text", nil)];
    [text setView:[self textView:inside]];
    [_tabView addTabViewItem:general];
    [_tabView addTabViewItem:text];
    [content addSubview:_tabView];

    NSButton *restore = [[NSButton alloc] initWithFrame:NSMakeRect(12, 12, 170, 28)];
    [restore setBezelStyle:NSRoundedBezelStyle];
    [restore setTitle:NSLocalizedString(@"Restore Defaults", nil)];
    [restore setTarget:self];
    [restore setAction:@selector(restoreDefaults:)];
    [content addSubview:restore];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(preferencesDidChange:)
        name:TMPreferencesDidChangeNotification object:nil];
    [self updateControls];
    [window center];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

/* ---- Values ---- */

- (void)updateControls
{
    TMPreferences *prefs = [TMPreferences sharedPreferences];
    NSUInteger scheme = [[TMPreferences schemeNames] indexOfObject:prefs.scheme];

    [_shellField setStringValue:prefs.shell];
    [_exitPopUp selectItemAtIndex:prefs.shellExitAction];
    [_columnsField setIntValue:prefs.columns];
    [_rowsField setIntValue:prefs.rows];
    [_scrollbackField setIntegerValue:prefs.scrollbackLines];
    [_bellPopUp selectItemAtIndex:prefs.bell];
    [_fontSizeField setIntegerValue:(NSInteger)prefs.fontSize];
    [_schemePopUp selectItemAtIndex:scheme == NSNotFound ? [_schemePopUp numberOfItems] - 1 : (NSInteger)scheme];
    [_backgroundWell setColor:prefs.backgroundColor];
    [_textWell setColor:prefs.textColor];
    [_accentCursorButton setState:prefs.cursorColor == nil ? NSOnState : NSOffState];
    [_cursorWell setColor:prefs.cursorColor ?: [NSColor controlAccentColor]];
    [_cursorWell setEnabled:prefs.cursorColor != nil];
    for (NSColorWell *well in _paletteWells)
        [well setColor:[prefs paletteColorAtIndex:[well tag]]];
}

- (void)preferencesDidChange:(NSNotification *)note
{
    [self updateControls];
}

- (IBAction)takeValue:(id)sender
{
    TMPreferences *prefs = [TMPreferences sharedPreferences];

    if (sender == _shellField)
        prefs.shell = [_shellField stringValue];
    else if (sender == _exitPopUp)
        prefs.shellExitAction = [_exitPopUp indexOfSelectedItem];
    else if (sender == _columnsField)
        prefs.columns = [_columnsField intValue];
    else if (sender == _rowsField)
        prefs.rows = [_rowsField intValue];
    else if (sender == _scrollbackField)
        prefs.scrollbackLines = [_scrollbackField integerValue];
    else if (sender == _bellPopUp)
        prefs.bell = [_bellPopUp indexOfSelectedItem];
    else if (sender == _fontSizeField)
        prefs.fontSize = [_fontSizeField doubleValue];
    else if (sender == _accentCursorButton)
        prefs.cursorColor = [_accentCursorButton state] == NSOnState ? nil : [_cursorWell color];
    /* A value out of range comes back clamped. */
    [self updateControls];
}

- (IBAction)takeScheme:(id)sender
{
    NSInteger index = [_schemePopUp indexOfSelectedItem];
    NSArray *names = [TMPreferences schemeNames];

    if (index >= 0 && (NSUInteger)index < [names count])
        [[TMPreferences sharedPreferences] applyScheme:names[index]];
    else
        [self updateControls];
}

- (IBAction)takeColor:(id)sender
{
    TMPreferences *prefs = [TMPreferences sharedPreferences];

    if (sender == _backgroundWell)
        prefs.backgroundColor = [sender color];
    else if (sender == _textWell)
        prefs.textColor = [sender color];
    else if (sender == _cursorWell)
        prefs.cursorColor = [sender color];
    else if ([_paletteWells containsObject:sender])
        [prefs setPaletteColor:[(NSColorWell *)sender color] atIndex:[(NSColorWell *)sender tag]];
}

- (IBAction)restoreDefaults:(id)sender
{
    [[TMPreferences sharedPreferences] restoreDefaults];
}

@end
