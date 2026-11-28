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

/* NSApplication.m: the main menu's first item made the app menu. */
@interface NSApplication (NSApplicationMenu)
-(void)_ensureApplicationMenu;
@end
#import <AppKit/NSWindow.h>
#import <AppKit/NSAlert.h>
#import <AppKit/NSEvent.h>

/* Minkar: the system menu's texts in the user's language (AppKit's own
   strings, es.lproj); they were English whatever the language. */
static NSString *L(NSString *text) {
    return [[NSBundle bundleForClass:[NSMenuBarWindow class]] localizedStringForKey:text value:text table:nil];
}
#include <servers/bootstrap.h>
#include <sys/sysctl.h>
#include "../../../CoreServices/loginwindow/LoginWindowRequest.h"
#include <LaunchServices/LaunchServices.h>

@interface NSMenu (NSMenuBarWindow)
- (NSString *)_name;
@end

static NSMutableArray *menuBars;	/* NSMenuBarWindow, one per screen */
static BOOL shown;
static BOOL updating;	/* setting the bars up changes the main menu too */

/*
 * The system menu, the same in every app (Atlas.md, MB0): About This
 * Computer, and Restart, Shut Down and Log Out as requests to the minimal
 * loginwindow (WindowServer plan, section 10.3). The other items act from
 * MB3; until then they are listed, disabled.
 */
@interface NSSystemMenuActions : NSObject <NSMenuDelegate>
@end

@implementation NSSystemMenuActions

/* SHORTCUT(Phase 5): a plain alert shows the computer; Atlas has its own
   About This Computer; taken in WS4b. */
- (void)aboutThisComputer:(id)sender {
    NSDictionary *version = [NSDictionary dictionaryWithContentsOfFile:
        @"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *name = [version objectForKey:@"ProductName"];
    NSString *shown = [version objectForKey:@"ProductUserVisibleVersion"];
    char cpu[128] = "", kernel[64] = "";
    size_t len = sizeof(cpu);
    uint64_t memory = 0;
    size_t memLen = sizeof(memory);

    sysctlbyname("machdep.cpu.brand_string", cpu, &len, NULL, 0);
    len = sizeof(kernel);
    sysctlbyname("kern.osrelease", kernel, &len, NULL, 0);
    sysctlbyname("hw.memsize", &memory, &memLen, NULL, 0);
    NSSize screen = [[NSScreen mainScreen] frame].size;

    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:[NSString stringWithFormat:@"%@ %@", name ? name : @"Minkar",
        shown ? shown : @""]];
    [alert setInformativeText:[NSString stringWithFormat:
        L(@"Kernel: Darwin %s\nProcessor: %s\nMemory: %.0f GB\nDisplay: %.0f × %.0f"),
        kernel, cpu, memory / 1073741824.0, screen.width, screen.height]];
    [alert addButtonWithTitle:L(@"OK")];
    /* The computer's icon, not the app's that shows the menu. */
    NSImage *computer = [NSImage imageNamed:NSImageNameComputer];
    if(computer != nil)
        [alert setIcon:computer];
    [alert runModal];
}

/* Sends a request to loginwindow, without waiting. */
static BOOL sendLoginWindowRequest(uint32_t request) {
    mach_port_t service = MACH_PORT_NULL;
    LoginWindowRequest msg;

    if(bootstrap_look_up(bootstrap_port, kLoginWindowServiceName, &service) != KERN_SUCCESS)
        return NO;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = service;
    msg.header.msgh_id = kLoginWindowRequestID;
    msg.request = request;
    kern_return_t kr = mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0,
        MACH_PORT_NULL, 1000, MACH_PORT_NULL);
    mach_port_deallocate(mach_task_self(), service);
    return kr == MACH_MSG_SUCCESS;
}

/* SHORTCUT(Phase 5): the app asks for confirmation; on the reference
   platform loginwindow does, with its countdown; taken in WS4b. Option
   held skips it, as there. */
- (void)request:(uint32_t)request question:(NSString *)question button:(NSString *)button {
    if(!([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask)) {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:L(question)];
        [alert setInformativeText:L(@"Applications that are open are asked to quit.")];
        [alert addButtonWithTitle:L(button)];
        [alert addButtonWithTitle:L(@"Cancel")];
        if([alert runModal] != NSAlertFirstButtonReturn)
            return;
    }
    if(!sendLoginWindowRequest(request))
        NSLog(@"loginwindow is not running: %@ not done", button);
}

- (void)restart:(id)sender {
    [self request:kLoginWindowRestart question:@"Are you sure you want to restart your computer now?"
        button:@"Restart"];
}

- (void)shutDown:(id)sender {
    [self request:kLoginWindowShutDown question:@"Are you sure you want to shut down your computer now?"
        button:@"Shut Down"];
}

/* System Preferences, as from the Dock. */
- (void)openPreferences:(id)sender {
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:[NSURL fileURLWithPath:@"/System/Applications/System Preferences.app"]
        options:NSWorkspaceLaunchDefault configuration:@{} error:NULL];
}

/* The Force Quit window: an app of its own, which loginwindow also opens
   for Command-Option-Escape. */
- (void)forceQuit:(id)sender {
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:[NSURL fileURLWithPath:@"/System/Library/CoreServices/ForceQuit.app"]
        options:NSWorkspaceLaunchDefault configuration:@{} error:NULL];
}

/* Sleep: the system, through power management (pmset sleepnow); a
   machine that cannot sleep yet (no ACPI S3) puts its displays to sleep
   instead. */
- (void)sleep:(id)sender {
    NSTask *task = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/pmset" arguments:@[ @"sleepnow" ]];
    [task waitUntilExit];
    if([task terminationStatus] != 0)
        [NSTask launchedTaskWithLaunchPath:@"/usr/bin/pmset" arguments:@[ @"displaysleepnow" ]];
}

/* Recent Items: the applications and documents LaunchServices' recent
   lists keep (AppKit notes each app as it launches and each document
   NSDocumentController notes), built as the menu opens. */
static NSArray *recentURLs(CFStringRef which) {
    LSSharedFileListRef list = LSSharedFileListCreate(NULL, which, NULL);
    NSMutableArray *urls = [NSMutableArray array];

    if(list == NULL)
        return urls;
    NSArray *items = [(NSArray *)LSSharedFileListCopySnapshot(list, NULL) autorelease];
    for(id object in items) {
        CFURLRef url = LSSharedFileListItemCopyResolvedURL((LSSharedFileListItemRef)object, 0, NULL);
        if(url != NULL) {
            if([[NSFileManager defaultManager] fileExistsAtPath:[(NSURL *)url path]])
                [urls addObject:(NSURL *)url];
            CFRelease(url);
        }
    }
    CFRelease(list);
    return urls;
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
    BOOL any = NO;

    [menu removeAllItems];
    for(NSArray *group in @[ @[ @"Applications", (id)kLSSharedFileListRecentApplicationItems ],
                             @[ @"Documents", (id)kLSSharedFileListRecentDocumentItems ] ]) {
        NSArray *urls = recentURLs((CFStringRef)[group objectAtIndex:1]);
        any |= [urls count] > 0;
        NSMenuItem *header = [menu addItemWithTitle:L([group objectAtIndex:0]) action:NULL keyEquivalent:@""];
        [header setEnabled:NO];
        for(NSURL *url in urls) {
            NSString *name = [[NSFileManager defaultManager] displayNameAtPath:[url path]];
            NSMenuItem *item = [menu addItemWithTitle:name action:@selector(openRecent:) keyEquivalent:@""];
            NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
            [icon setSize:NSMakeSize(16, 16)];
            [item setImage:icon];
            [item setTarget:self];
            [item setRepresentedObject:url];
            [item setEnabled:YES];
        }
        [menu addItem:[NSMenuItem separatorItem]];
    }
    NSMenuItem *clear = [menu addItemWithTitle:L(@"Clear Menu") action:@selector(clearRecent:) keyEquivalent:@""];
    [clear setTarget:self];
    [clear setEnabled:any];	/* nothing to clear: dimmed, as on the reference */
}

- (void)openRecent:(NSMenuItem *)sender {
    [[NSWorkspace sharedWorkspace] openURL:[sender representedObject]];
}

- (void)clearRecent:(id)sender {
    for(id which in @[ (id)kLSSharedFileListRecentApplicationItems, (id)kLSSharedFileListRecentDocumentItems ]) {
        LSSharedFileListRef list = LSSharedFileListCreate(NULL, (CFStringRef)which, NULL);
        if(list != NULL) {
            LSSharedFileListRemoveAllItems(list);
            CFRelease(list);
        }
    }
}

/* Lock Screen: loginwindow covers the session until its user's password
   is given; no confirmation, as on the reference platform. */
- (void)lockScreen:(id)sender {
    if(!sendLoginWindowRequest(kLoginWindowLockScreen))
        NSLog(@"loginwindow is not running: Lock Screen not done");
}

- (void)logOut:(id)sender {
    [self request:kLoginWindowLogOut question:@"Are you sure you want to quit all applications and log out now?"
        button:@"Log Out"];
}

@end

static NSMenuItem *systemMenuItem(void) {
    static NSMenuItem *item;
    static NSSystemMenuActions *actions;

    if(item != nil)
        return item;
    actions = [NSSystemMenuActions new];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [menu setAutoenablesItems:NO];
    NSString *logOut = [NSString stringWithFormat:L(@"Log Out %@…"), NSFullUserName()];
    /* Title, action, key equivalent (Command plus, unless other modifiers
       are named: o Option, c Control). */
    NSString *escape = [NSString stringWithFormat:@"%C", (unichar)27];
    NSArray *entries = @[
        @[ @"About This Computer", @"aboutThisComputer:" ], @[ @"-" ],
        @[ @"System Preferences…", @"openPreferences:" ], @[ @"-" ],
        @[ @"Recent Items", @"recent" ], @[ @"-" ],
        @[ @"Force Quit…", @"forceQuit:", escape, @"o" ], @[ @"-" ],
        @[ @"Sleep", @"sleep:" ], @[ @"Restart…", @"restart:" ], @[ @"Shut Down…", @"shutDown:" ], @[ @"-" ],
        @[ @"Lock Screen", @"lockScreen:", @"q", @"c" ], @[ logOut, @"logOut:", @"Q", @"" ] ];
    /* The installation medium has no session to leave, lock or set up:
       only the computer, and restarting or shutting it down. */
    if([[NSFileManager defaultManager] fileExistsAtPath:@"/private/var/db/.MinkarInstallMedium"])
        entries = @[ @[ @"About This Computer", @"aboutThisComputer:" ], @[ @"-" ],
            @[ @"Restart…", @"restart:" ], @[ @"Shut Down…", @"shutDown:" ] ];
    for(NSArray *entry in entries) {
        NSString *title = [entry objectAtIndex:0];
        if(entry != [entries lastObject] || ![title isEqualToString:logOut])
            title = L(title);
        if([title isEqualToString:@"-"]) {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        NSString *action = [entry objectAtIndex:1];
        if([action isEqualToString:@"recent"]) {
            NSMenu *recent = [[[NSMenu alloc] initWithTitle:title] autorelease];
            [recent setAutoenablesItems:NO];
            [recent setDelegate:actions];
            [[menu addItemWithTitle:title action:NULL keyEquivalent:@""] setSubmenu:recent];
            continue;
        }
        NSString *key = [entry count] > 2 ? [entry objectAtIndex:2] : @"";
        NSMenuItem *it = [menu addItemWithTitle:title
            action:[action length] ? NSSelectorFromString(action) : NULL keyEquivalent:key];
        if([entry count] > 3) {
            NSString *mods = [entry objectAtIndex:3];
            NSUInteger mask = NSCommandKeyMask;
            if([mods rangeOfString:@"o"].location != NSNotFound) mask |= NSAlternateKeyMask;
            if([mods rangeOfString:@"c"].location != NSNotFound) mask |= NSControlKeyMask;
            [it setKeyEquivalentModifierMask:mask];
        }
        [it setTarget:actions];
        [it setEnabled:[action length] > 0];
    }
    item = [[NSMenuItem alloc] initWithTitle:@"" action:NULL keyEquivalent:@""];
    NSString *path = [[NSBundle bundleForClass:[NSMenuBarWindow class]]
        pathForResource:@"MinkarMenu-Black-18" ofType:@"png"];
    NSImage *mark = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
    if(mark != nil) {
        /* A template: drawn in the menu bar's text color, light or dark. */
        [mark setTemplate:YES];
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

- (instancetype)_initBackdropWithScreen:(NSScreen *)screen {
    NSRect frame = barFrame(screen);
    NSMenu *empty = [[[NSMenu alloc] initWithTitle:@""] autorelease];

    self = [super initWithContentRect:frame styleMask:NSBorderlessWindowMask
        backing:NSBackingStoreBuffered defer:NO];
    if(self == nil)
        return nil;
    /* The level below the bars' (the Dock's): above every app's window. */
    [self setLevel:NSDockWindowLevel];
    [self setHasShadow:NO];
    [self setMovable:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setHidesOnDeactivate:NO];
    [self setIgnoresMouseEvents:YES];
    _view = [[NSMenuBarView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width,
        frame.size.height) menu:empty];
    [_view setFrameSize:NSMakeSize(frame.size.width, frame.size.height)];
    [_view setAutoresizingMask:NSViewWidthSizable];
    [_view setLeadingItem:systemMenuItem()];
    [[self contentView] addSubview:_view];
    return self;
}

+ (NSWindow *)_backdropForScreen:(NSScreen *)screen {
    return [[[self alloc] _initBackdropWithScreen:screen] autorelease];
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

/* An item added to, removed from or changed in the main menu shows on
 * the bars at once. */
+ (void)_menuChanged:(NSNotification *)note {
    if([note object] == [NSApp mainMenu])
        [self _mainMenuChanged];
}

+ (void)_showMenuBars {
    NSArray *screens = [NSScreen screens];
    static BOOL observing;

    if(!observing) {
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
        for(NSString *name in @[ NSMenuDidAddItemNotification, NSMenuDidRemoveItemNotification,
                NSMenuDidChangeItemNotification ])
            [center addObserver:self selector:@selector(_menuChanged:) name:name object:nil];
        observing = YES;
    }

    [NSApp _ensureApplicationMenu];
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
    updating = YES;
    for(NSMenuBarWindow *bar in menuBars) {
        [bar _setMainMenu:[NSApp mainMenu]];
        [bar orderFront:nil];
    }
    updating = NO;
    shown = YES;
}

/* A display changed: each bar takes its screen's new top edge; a bar
 * left without a screen is put away. */
+ (void)_screensChanged {
    NSArray *screens = [NSScreen screens];

    for(NSUInteger i = 0; i < [menuBars count]; i++) {
        NSMenuBarWindow *bar = [menuBars objectAtIndex:i];
        if(i < [screens count]) {
            [bar setFrame:barFrame([screens objectAtIndex:i]) display:shown];
            if(shown)
                [bar orderFront:nil];
        } else
            [bar orderOut:nil];
    }
    if(shown)
        [self _showMenuBars];	/* a bar for a screen that came */
}

+ (void)_hideMenuBars {
    for(NSMenuBarWindow *bar in menuBars)
        [bar orderOut:nil];
    shown = NO;
}

+ (BOOL)_performSystemMenuKeyEquivalent:(NSEvent *)event {
    return [[systemMenuItem() submenu] performKeyEquivalent:event];
}

+ (void)_focusMenuBar {
    if(!shown || [menuBars count] == 0)
        return;
    NSMenuBarWindow *bar = [menuBars objectAtIndex:0];
    [bar->_view _trackFromKeyboard];
}

+ (void)_mainMenuChanged {
    if(!shown || updating)
        return;
    [NSApp _ensureApplicationMenu];
    updating = YES;
    for(NSMenuBarWindow *bar in menuBars)
        [bar _setMainMenu:[NSApp mainMenu]];
    updating = NO;
}

@end
