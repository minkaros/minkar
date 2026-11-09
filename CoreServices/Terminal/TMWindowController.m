/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMWindowController.h"
#import "TMPreferences.h"
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>

@implementation TMWindowController {
    NSTabView *_tabs;
}

- (instancetype)initWithArguments:(NSArray<NSString *> *)arguments
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 680, 440)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];

    if ((self = [super initWithWindow:window]) == nil)
        return nil;
    [window setDelegate:self];
    [window setMinSize:NSMakeSize(300, 160)];
    [window setReleasedWhenClosed:NO];
    _tabs = [[NSTabView alloc] initWithFrame:[[window contentView] bounds]];
    [_tabs setTabViewType:NSNoTabsNoBorder];
    [_tabs setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_tabs setDelegate:self];
    [[window contentView] addSubview:_tabs];
    TMTerminalView *first = [self addTabWithArguments:arguments];
    if (first != nil) {
        /* The size the preferences ask for, in rows and columns. */
        TMPreferences *prefs = [TMPreferences sharedPreferences];
        NSSize content = [first contentSizeForRows:prefs.rows columns:prefs.columns];
        [window setContentSize:[NSScrollView frameSizeForContentSize:content hasHorizontalScroller:NO
            hasVerticalScroller:YES borderType:NSNoBorder]];
        [self updateTabBar];
        [self updateTitles];
    }
    [window cascadeTopLeftFromPoint:NSZeroPoint];
    [window center];
    return self;
}

- (TMTerminalView *)addTabWithArguments:(NSArray<NSString *> *)arguments
{
    NSRect frame = [_tabs contentRect];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
    TMTerminalView *terminal = [[TMTerminalView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame))
        arguments:arguments];
    NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:terminal];
    __weak TMWindowController *weakSelf = self;

    if (terminal.session == nil)
        return nil;
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSNoBorder];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scroll setDocumentView:terminal];
    [item setView:scroll];
    terminal.titleChanged = ^(TMTerminalView *view) {
        [weakSelf updateTitles];
    };
    terminal.exited = ^(TMTerminalView *view, int status) {
        [weakSelf terminal:view exitedWithStatus:status];
    };
    [_tabs addTabViewItem:item];
    [_tabs selectTabViewItem:item];
    [self updateTabBar];
    [terminal fitToScrollView];
    [[self window] makeFirstResponder:terminal];
    [self updateTitles];
    return terminal;
}

- (NSArray<TMTerminalView *> *)terminals
{
    return [[_tabs tabViewItems] valueForKey:@"identifier"];
}

- (TMTerminalView *)currentTerminal
{
    return [[_tabs selectedTabViewItem] identifier];
}

/* The tab bar shows from the second tab on. */
- (void)updateTabBar
{
    [_tabs setTabViewType:[_tabs numberOfTabViewItems] > 1 ? NSTopTabsBezelBorder : NSNoTabsNoBorder];
    for (NSTabViewItem *item in [_tabs tabViewItems]) {
        TMTerminalView *terminal = [item identifier];
        [[terminal enclosingScrollView] setFrame:[_tabs contentRect]];
        [terminal fitToScrollView];
    }
}

/* "user — zsh — 80×24", or the title the program set. */
- (NSString *)titleOf:(TMTerminalView *)terminal
{
    TMSession *s = terminal.session;
    struct passwd *pw = getpwuid(getuid());

    if ([s.title length])
        return s.title;
    return [NSString stringWithFormat:@"%s — %@ — %d×%d", pw ? pw->pw_name : "?",
        s.programName ?: @"sh", s.columns, s.rows];
}

- (void)updateTitles
{
    for (NSTabViewItem *item in [_tabs tabViewItems])
        [item setLabel:[self titleOf:[item identifier]]];
    if ([self currentTerminal] != nil)
        [[self window] setTitle:[self titleOf:[self currentTerminal]]];
}

/* As the preferences say: close the tab, close it if the program ended
 * well, or keep it with a line saying how it ended. */
- (void)terminal:(TMTerminalView *)terminal exitedWithStatus:(int)status
{
    BOOL clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    switch ([TMPreferences sharedPreferences].shellExitAction) {
    case TMShellExitClose:
        [self closeTerminal:terminal];
        return;
    case TMShellExitCloseIfClean:
        if (clean) {
            [self closeTerminal:terminal];
            return;
        }
        break;
    case TMShellExitKeepOpen:
        break;
    }
    [terminal.session showMessage:clean ? NSLocalizedString(@"[Process completed]", nil) :
        [NSString stringWithFormat:NSLocalizedString(@"[Process exited with status %d]", nil),
            WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)]];
    [self updateTitles];
}

- (void)closeTerminal:(TMTerminalView *)terminal
{
    for (NSTabViewItem *item in [_tabs tabViewItems])
        if ([item identifier] == terminal) {
            [terminal.session terminate];
            [_tabs removeTabViewItem:item];
            break;
        }
    if ([_tabs numberOfTabViewItems] == 0) {
        [[self window] close];
        return;
    }
    [self updateTabBar];
    [[self window] makeFirstResponder:[self currentTerminal]];
    [self updateTitles];
}

- (void)windowDidResize:(NSNotification *)note
{
    for (TMTerminalView *terminal in [self terminals])
        [terminal fitToScrollView];
    [self updateTitles];
}

- (void)windowWillClose:(NSNotification *)note
{
    for (TMTerminalView *terminal in [self terminals])
        [terminal.session terminate];
    if (self.closed)
        self.closed(self);
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)item
{
    [[item identifier] fitToScrollView];
    [[self window] makeFirstResponder:[item identifier]];
    [self updateTitles];
}

- (IBAction)newTab:(id)sender
{
    [self addTabWithArguments:nil];
}

- (IBAction)closeTab:(id)sender
{
    if ([self currentTerminal] != nil)
        [self closeTerminal:[self currentTerminal]];
}

- (IBAction)selectNextTab:(id)sender
{
    NSInteger i = [_tabs indexOfTabViewItem:[_tabs selectedTabViewItem]];
    [_tabs selectTabViewItemAtIndex:(i + 1) % [_tabs numberOfTabViewItems]];
}

- (IBAction)selectPreviousTab:(id)sender
{
    NSInteger n = [_tabs numberOfTabViewItems], i = [_tabs indexOfTabViewItem:[_tabs selectedTabViewItem]];
    [_tabs selectTabViewItemAtIndex:(i - 1 + n) % n];
}

/* The preference: every window takes it. */
- (void)setTextSize:(CGFloat)size
{
    [TMPreferences sharedPreferences].fontSize = size;
}

- (IBAction)biggerText:(id)sender
{
    [self setTextSize:[self currentTerminal].fontSize + 1];
}

- (IBAction)smallerText:(id)sender
{
    [self setTextSize:[self currentTerminal].fontSize - 1];
}

- (IBAction)defaultTextSize:(id)sender
{
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:TMFontSizeKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:TMPreferencesDidChangeNotification object:nil];
}

@end
