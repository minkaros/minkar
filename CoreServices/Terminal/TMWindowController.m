/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMWindowController.h"
#include <pwd.h>
#include <unistd.h>

#define DEFAULT_SIZE 13.0

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
    [self addTabWithArguments:arguments];
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
        [weakSelf closeTerminal:view];
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
        [[NSString stringWithUTF8String:pw && pw->pw_shell ? pw->pw_shell : "sh"] lastPathComponent], s.columns, s.rows];
}

- (void)updateTitles
{
    for (NSTabViewItem *item in [_tabs tabViewItems])
        [item setLabel:[self titleOf:[item identifier]]];
    if ([self currentTerminal] != nil)
        [[self window] setTitle:[self titleOf:[self currentTerminal]]];
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

- (void)setTextSize:(CGFloat)size
{
    for (TMTerminalView *terminal in [self terminals])
        terminal.fontSize = MAX(8, MIN(36, size));
    [[NSUserDefaults standardUserDefaults] setDouble:[self currentTerminal].fontSize forKey:@"FontSize"];
    [self updateTitles];
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
    [self setTextSize:DEFAULT_SIZE];
}

@end
