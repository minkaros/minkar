/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import "NSMenuBarWindow.h"
#import "NSMenuBarView.h"
#import <AppKit/NSApplication.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSFontManager.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSWindow.h>

@interface NSMenu (NSMenuBarWindow)
- (NSString *)_name;
@end

static NSMutableArray *menuBars;	/* NSMenuBarWindow, one per screen */
static BOOL shown;

/*
 * The system menu, the same in every app: its items act with the minimal
 * loginwindow (MB0, WindowServer plan section 10.3); until then they are
 * listed, disabled.
 */
static NSMenuItem *systemMenuItem(void) {
    static NSMenuItem *item;

    if(item != nil)
        return item;
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [menu setAutoenablesItems:NO];
    NSArray *titles = @[ @"About This Computer", @"System Preferences…", @"-", @"Force Quit…",
        @"-", @"Sleep", @"Restart…", @"Shut Down…", @"-", @"Lock Screen", @"Log Out" ];
    for(NSString *title in titles) {
        if([title isEqualToString:@"-"]) {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        NSMenuItem *entry = [menu addItemWithTitle:title action:NULL keyEquivalent:@""];
        [entry setEnabled:NO];
    }
    item = [[NSMenuItem alloc] initWithTitle:@"" action:NULL keyEquivalent:@""];
    NSString *path = [[NSBundle bundleForClass:[NSMenuBarWindow class]]
        pathForResource:@"MinkarMenu-Black-18" ofType:@"png"];
    NSImage *mark = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
    if(mark != nil) {
        [mark setScalesWhenResized:YES];
        [mark setSize:NSMakeSize(16, 16)];
        [item setImage:mark];
        [mark release];
    } else
        [item setTitle:@"Minkar"];
    [item setSubmenu:menu];
    [menu release];
    return item;
}

/* The app menu's title, in bold (the app's name). */
static void boldAppMenu(NSMenu *mainMenu) {
    for(NSMenuItem *item in [mainMenu itemArray]) {
        if(![item hasSubmenu] || ![[[item submenu] _name] isEqualToString:@"NSAppleMenu"])
            continue;
        NSFont *bold = [[NSFontManager sharedFontManager] convertFont:[NSFont menuFontOfSize:15]
            toHaveTrait:NSBoldFontMask];
        NSAttributedString *title = [[NSAttributedString alloc] initWithString:[item title]
            attributes:[NSDictionary dictionaryWithObject:bold forKey:NSFontAttributeName]];
        [item setAttributedTitle:title];
        [title release];
        return;
    }
}

@implementation NSMenuBarWindow {
    NSMenuBarView *_view;
}

/* The strip at the top of a screen that its visible frame leaves out. */
static NSRect barFrame(NSScreen *screen) {
    NSRect frame = [screen frame], visible = [screen visibleFrame];
    CGFloat height = NSMaxY(frame) - NSMaxY(visible);

    if(height <= 0)
        height = 22;
    return NSMakeRect(frame.origin.x, NSMaxY(frame) - height, frame.size.width, height);
}

- (instancetype)initWithScreen:(NSScreen *)screen {
    NSRect frame = barFrame(screen);

    self = [super initWithContentRect:frame styleMask:NSBorderlessWindowMask
        backing:NSBackingStoreBuffered defer:NO];
    if(self == nil)
        return nil;
    [self setLevel:NSMainMenuWindowLevel];
    [self setHasShadow:NO];
    [self setMovable:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setHidesOnDeactivate:NO];
    _view = [[NSMenuBarView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width,
        frame.size.height) menu:[NSApp mainMenu]];
    [_view setFrameSize:NSMakeSize(frame.size.width, frame.size.height)];
    [_view setAutoresizingMask:NSViewWidthSizable];
    [_view setLeadingItem:systemMenuItem()];
    [[self contentView] addSubview:_view];
    return self;
}

- (void)dealloc {
    [_view release];
    [super dealloc];
}

/* A menu bar is never key or main: the app's windows keep their state. */
- (BOOL)canBecomeKeyWindow {
    return NO;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}

/* The visible frame leaves the menu bar out: no constraining to it. */
- (NSRect)constrainFrameRect:(NSRect)rect toScreen:(NSScreen *)screen {
    return rect;
}

- (void)_setMainMenu:(NSMenu *)menu {
    boldAppMenu(menu);
    [_view setMenu:menu];
    [_view setFrameSize:NSMakeSize([self frame].size.width, [self frame].size.height)];
    [_view setNeedsDisplay:YES];
}

+ (void)_showMenuBars {
    NSArray *screens = [NSScreen screens];

    if(menuBars == nil)
        menuBars = [NSMutableArray new];
    /* One per screen, made once. */
    while([menuBars count] < [screens count]) {
        NSMenuBarWindow *bar = [[NSMenuBarWindow alloc]
            initWithScreen:[screens objectAtIndex:[menuBars count]]];
        if(bar == nil)
            break;
        [menuBars addObject:bar];
        [bar release];
    }
    for(NSMenuBarWindow *bar in menuBars) {
        [bar _setMainMenu:[NSApp mainMenu]];
        [bar orderFront:nil];
    }
    shown = YES;
}

+ (void)_hideMenuBars {
    for(NSMenuBarWindow *bar in menuBars)
        [bar orderOut:nil];
    shown = NO;
}

+ (void)_mainMenuChanged {
    if(!shown)
        return;
    for(NSMenuBarWindow *bar in menuBars)
        [bar _setMainMenu:[NSApp mainMenu]];
}

@end
