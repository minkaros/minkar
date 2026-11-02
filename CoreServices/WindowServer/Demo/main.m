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

/*
 * One titled window whose content is filled blue. Prints, one line each:
 *
 *   window frame=X,Y,W,H              (AppKit coordinates, from bottom left)
 *   event keyDown keyCode=K chars=C
 *   event keyUp keyCode=K
 *   event mouseDown x=X y=Y           (in the content view's coordinates)
 *   event mouseUp
 *   app active | app inactive         (NSApplication's notifications)
 *   workspace launched|terminated|activated|deactivated pid=P id=ID
 *                                     (NSWorkspace's, about any app)
 */

#import <AppKit/AppKit.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

@interface DemoView : NSView
@property (strong) NSColor *color;
@property BOOL beamCursor;
@end

@implementation DemoView

- (BOOL)acceptsFirstResponder {
    return YES;
}

/* "cursorrect": an I-beam over the whole view, as text views have. */
- (void)resetCursorRects {
    if (self.beamCursor)
        [self addCursorRect:[self bounds] cursor:[NSCursor IBeamCursor]];
}

- (void)drawRect:(NSRect)rect {
    NSColor *color = self.color;
    if (color == nil)
        color = [NSColor colorWithCalibratedRed:0.1 green:0.3 blue:0.9 alpha:1];
    [color set];
    NSRectFill([self bounds]);
}

- (void)keyDown:(NSEvent *)event {
    printf("event keyDown keyCode=%u chars=%s\n", (unsigned)[event keyCode],
        [[event characters] UTF8String]);
}

- (void)keyUp:(NSEvent *)event {
    printf("event keyUp keyCode=%u\n", (unsigned)[event keyCode]);
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    printf("event mouseDown x=%.0f y=%.0f\n", p.x, p.y);
}

- (void)mouseUp:(NSEvent *)event {
    printf("event mouseUp\n");
}

@end

@interface DemoDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>
@property (strong) NSWindow *window;
@property (strong) NSWindow *second;
@property (copy) NSString *commandPath;
@property (strong) NSWindow *child;
@property (strong) NSWindow *sheet;
@property (strong) NSMenu *contextMenu;
@property (strong) NSPopUpButton *popUp;
@property (strong) NSStatusItem *statusItem;
@property BOOL refusesToQuit;
@end

@implementation DemoDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(200, 200, 400, 300)
        styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
    [self.window setTitle:@"Demo"];
    DemoView *view = [[DemoView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300)];
    [self.window setContentView:view];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:view];
    NSRect f = [self.window frame];
    printf("window frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y,
        f.size.width, f.size.height);
    [self observeApplications];

    /*
     * -two: a red window over the blue one, and commands, one at a time,
     * from /tmp/ws-demo.cmd (deleted when done): "raise-blue", "move-red X
     * Y" (its frame's origin), "movable-red" (movable by its background),
     * "busy N" (the app does not answer for N seconds; the window server
     * still moves its windows), "quit". For the window server's tests.
     * "-cmd PATH" reads the commands from PATH instead, with one window,
     * so two instances can run; then also "apps" (the running apps),
     * "activate PID" (asks that app to activate), "screens" (each
     * screen's frame and visible frame, and the menu bar's height),
     * "minimize" and "restore" (its window), "child" and "unchild" (a
     * green child window over its right edge), "sheet" and "endsheet",
     * "cursorrect" (an I-beam cursor over its blue view), "ibeam" (sets
     * the I-beam cursor), "menus" (a Test menu: Alpha, Beta disabled
     * by validation, a More submenu with Gamma, and a delegate; each
     * prints what happens; key equivalents Command-A, Shift-Command-B
     * and Option-Command-G), "keyequivs" (how menus show some key
     * equivalents), "context" (a context menu on the blue view: One, Two,
     * a Three submenu with Four), "popup" (that menu opened at the view's
     * middle by popUpMenuPositioningItem:, Two placed there), "popupbutton"
     * (a pop-up button: Red, Green, Blue), "statusitem" (a status item
     * "Demo" with a menu: Status One, Status Two), "nostatusitem" (removes
     * it), "refusequit" (its delegate refuses to quit from now on), "tags"
     * (the Test menu's item tags; Alpha's is 42).
     */
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSUInteger cmd = [args indexOfObject:@"-cmd"];
    self.commandPath = @"/tmp/ws-demo.cmd";
    if (cmd != NSNotFound && cmd + 1 < [args count]) {
        self.commandPath = [args objectAtIndex:cmd + 1];
        [NSTimer scheduledTimerWithTimeInterval:0.05 target:self
            selector:@selector(readCommand:) userInfo:nil repeats:YES];
        return;
    }
    if (![args containsObject:@"-two"])
        return;
    self.second = [[NSWindow alloc] initWithContentRect:NSMakeRect(450, 100, 300, 250)
        styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
    [self.second setTitle:@"Second"];
    DemoView *red = [[DemoView alloc] initWithFrame:NSMakeRect(0, 0, 300, 250)];
    red.color = [NSColor colorWithCalibratedRed:0.9 green:0.1 blue:0.1 alpha:1];
    [self.second setContentView:red];
    [self.second orderFront:nil];
    f = [self.second frame];
    printf("second frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y,
        f.size.width, f.size.height);
    [NSTimer scheduledTimerWithTimeInterval:0.05 target:self
        selector:@selector(readCommand:) userInfo:nil repeats:YES];
    /* Moved by its title bar: the frame AppKit was told. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(secondMoved:)
        name:NSWindowDidMoveNotification object:self.second];
}

/* LaunchServices' session, as AppKit shows it (WS4a). */
- (void)observeApplications {
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    NSDictionary *names = @{
        NSWorkspaceDidLaunchApplicationNotification: @"launched",
        NSWorkspaceDidTerminateApplicationNotification: @"terminated",
        NSWorkspaceDidActivateApplicationNotification: @"activated",
        NSWorkspaceDidDeactivateApplicationNotification: @"deactivated",
    };
    for (NSString *name in names)
        [center addObserverForName:name object:nil queue:nil usingBlock:^(NSNotification *note) {
            NSRunningApplication *app = [[note userInfo] objectForKey:NSWorkspaceApplicationKey];
            printf("workspace %s pid=%d id=%s\n", [[names objectForKey:[note name]] UTF8String],
                [app processIdentifier], [[app bundleIdentifier] UTF8String]);
        }];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(becameActive:)
        name:NSApplicationDidBecomeActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(resignedActive:)
        name:NSApplicationDidResignActiveNotification object:nil];
}

- (void)becameActive:(NSNotification *)note {
    printf("app active\n");
}

- (void)resignedActive:(NSNotification *)note {
    printf("app inactive\n");
}

- (void)printApplications {
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSRunningApplication *me = [NSRunningApplication currentApplication];

    for (NSRunningApplication *app in [workspace runningApplications])
        printf("app pid=%d id=%s name=%s active=%d policy=%ld\n", [app processIdentifier],
            [[app bundleIdentifier] UTF8String], [[app localizedName] UTF8String],
            [app isActive], (long)[app activationPolicy]);
    printf("frontmost pid=%d\n", [[workspace frontmostApplication] processIdentifier]);
    printf("current pid=%d id=%s\n", [me processIdentifier], [[me bundleIdentifier] UTF8String]);
}

- (void)secondMoved:(NSNotification *)note {
    NSRect f = [self.second frame];
    printf("second moved frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y,
        f.size.width, f.size.height);
}

/* The Test menu (MB0): actions, validation, a submenu and a delegate. */
- (void)addTestMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Test"];
    [menu setDelegate:self];
    NSMenuItem *alpha = [menu addItemWithTitle:@"Alpha" action:@selector(menuAlpha:) keyEquivalent:@"a"];
    [alpha setTarget:self];
    [alpha setTag:42];
    [[menu addItemWithTitle:@"Beta" action:@selector(menuBeta:) keyEquivalent:@"B"]
        setTarget:self];
    NSMenu *more = [[NSMenu alloc] initWithTitle:@"More"];
    [more setDelegate:self];
    NSMenuItem *gamma = [more addItemWithTitle:@"Gamma" action:@selector(menuGamma:) keyEquivalent:@"g"];
    [gamma setTarget:self];
    [gamma setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
    [[menu addItemWithTitle:@"More" action:NULL keyEquivalent:@""] setSubmenu:more];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Test" action:NULL keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];
    for (NSString *name in @[ NSMenuDidBeginTrackingNotification, NSMenuDidEndTrackingNotification,
            NSMenuDidSendActionNotification ])
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(menuNote:)
            name:name object:nil];
    printf("menus added\n");
}

/* How menus show some key equivalents (AppKit's private description). */
- (void)printKeyEquivalents {
    struct { NSString *key; NSUInteger mask; } cases[] = {
        { @"a", NSCommandKeyMask },
        { @"B", NSCommandKeyMask },
        { @"g", NSCommandKeyMask | NSAlternateKeyMask },
        { @"\r", NSCommandKeyMask },
        { @"\x1b", NSCommandKeyMask | NSAlternateKeyMask },
        { [NSString stringWithFormat:@"%C", (unichar)NSF5FunctionKey], 0 },
        { [NSString stringWithFormat:@"%C", (unichar)NSLeftArrowFunctionKey], NSControlKeyMask | NSShiftKeyMask },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"x" action:NULL keyEquivalent:cases[i].key];
        [item setKeyEquivalentModifierMask:cases[i].mask];
        printf("keyequiv %s\n", [[item performSelector:@selector(_keyEquivalentDescription)] UTF8String]);
    }
}

- (void)addContextMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Context"];
    [menu setDelegate:self];
    for (NSString *title in @[ @"One", @"Two" ])
        [[menu addItemWithTitle:title action:@selector(contextItem:) keyEquivalent:@""] setTarget:self];
    NSMenu *three = [[NSMenu alloc] initWithTitle:@"Three"];
    [[three addItemWithTitle:@"Four" action:@selector(contextItem:) keyEquivalent:@""] setTarget:self];
    [[menu addItemWithTitle:@"Three" action:NULL keyEquivalent:@""] setSubmenu:three];
    self.contextMenu = menu;
    [[self.window contentView] setMenu:menu];
    printf("context menu set\n");
}

- (void)contextItem:(NSMenuItem *)item {
    printf("context %s\n", [[item title] UTF8String]);
}

- (void)addPopUpButton {
    NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(20, 20, 140, 26) pullsDown:NO];
    [button addItemsWithTitles:@[ @"Red", @"Green", @"Blue" ]];
    [button setTarget:self];
    [button setAction:@selector(popUpChanged:)];
    [[self.window contentView] addSubview:button];
    self.popUp = button;
    NSRect f = [button convertRect:[button bounds] toView:nil];
    f.origin = [self.window convertBaseToScreen:f.origin];
    printf("popup button frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y, f.size.width, f.size.height);
}

- (void)popUpChanged:(NSPopUpButton *)sender {
    printf("popup button selected %s\n", [[sender titleOfSelectedItem] UTF8String]);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    printf("asked to quit: %s\n", self.refusesToQuit ? "refused" : "quitting");
    return self.refusesToQuit ? NSTerminateCancel : NSTerminateNow;
}

- (void)addStatusItem {
    NSStatusItem *item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    item.button.title = @"Demo";
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Status"];
    for (NSString *title in @[ @"Status One", @"Status Two" ])
        [[menu addItemWithTitle:title action:@selector(statusChosen:) keyEquivalent:@""] setTarget:self];
    item.menu = menu;
    self.statusItem = item;
    printf("status item added length=%.0f\n", [item length]);
}

- (void)statusChosen:(NSMenuItem *)sender {
    printf("status %s\n", [[sender title] UTF8String]);
}

- (void)menuAlpha:(id)sender { printf("menu alpha\n"); }
- (void)menuBeta:(id)sender { printf("menu beta\n"); }
- (void)menuGamma:(id)sender { printf("menu gamma\n"); }

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    return [item action] != @selector(menuBeta:);
}

- (void)menuNeedsUpdate:(NSMenu *)menu { printf("menu needs update %s\n", [[menu title] UTF8String]); }
- (void)menuWillOpen:(NSMenu *)menu { printf("menu will open %s\n", [[menu title] UTF8String]); }
- (void)menuDidClose:(NSMenu *)menu { printf("menu did close %s\n", [[menu title] UTF8String]); }
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item {
    if (item != nil)
        printf("menu highlight %s\n", [[item title] UTF8String]);
}

- (void)menuNote:(NSNotification *)note {
    printf("menu note %s\n", [[note name] UTF8String]);
}

- (void)readCommand:(NSTimer *)timer {
    const char *path = [self.commandPath fileSystemRepresentation];
    char line[64] = "";
    double x, y;
    int pid;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return;
    if (fgets(line, sizeof(line), f) == NULL)
        line[0] = '\0';
    fclose(f);
    unlink(path);
    if (strncmp(line, "raise-blue", 10) == 0)
        [self.window orderFront:nil];
    else if (sscanf(line, "move-red %lf %lf", &x, &y) == 2)
        [self.second setFrameOrigin:NSMakePoint(x, y)];
    else if (strncmp(line, "movable-red", 11) == 0) {
        [self.second setMovableByWindowBackground:YES];
        [self.second display];
    } else if (sscanf(line, "busy %lf", &x) == 1) {
        printf("busy for %.0f s\n", x);
        usleep((useconds_t)(x * 1000000));
        printf("busy done\n");
    } else if (strncmp(line, "apps", 4) == 0)
        [self printApplications];
    else if (strncmp(line, "minimize", 8) == 0) {
        [self.window miniaturize:nil];
        printf("minimized=%d\n", [self.window isMiniaturized]);
    } else if (strncmp(line, "restore", 7) == 0) {
        [self.window deminiaturize:nil];
        printf("minimized=%d\n", [self.window isMiniaturized]);
    } else if (strncmp(line, "unchild", 7) == 0) {
        [self.window removeChildWindow:self.child];
        printf("children=%lu\n", (unsigned long)[[self.window childWindows] count]);
    } else if (strncmp(line, "child", 5) == 0) {
        NSRect f = [self.window frame];
        self.child = [[NSWindow alloc] initWithContentRect:NSMakeRect(NSMaxX(f) - 60, f.origin.y + 40, 120, 80)
            styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
        DemoView *green = [[DemoView alloc] initWithFrame:NSMakeRect(0, 0, 120, 80)];
        green.color = [NSColor colorWithCalibratedRed:0.1 green:0.8 blue:0.2 alpha:1];
        [self.child setContentView:green];
        [self.window addChildWindow:self.child ordered:NSWindowAbove];
        printf("children=%lu\n", (unsigned long)[[self.window childWindows] count]);
    } else if (strncmp(line, "endsheet", 8) == 0) {
        [NSApp endSheet:self.sheet];
        [self.sheet orderOut:nil];
        printf("sheet ended\n");
    } else if (strncmp(line, "sheet", 5) == 0) {
        self.sheet = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 240, 120)
            styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
        [NSApp beginSheet:self.sheet modalForWindow:self.window modalDelegate:nil
            didEndSelector:NULL contextInfo:NULL];
        NSRect f = [self.sheet frame];
        printf("sheet frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y,
            f.size.width, f.size.height);
    } else if (strncmp(line, "ibeam", 5) == 0) {
        [[NSCursor IBeamCursor] set];
        printf("cursor set\n");
    } else if (strncmp(line, "cursorrect", 10) == 0) {
        ((DemoView *)[self.window contentView]).beamCursor = YES;
        [self.window invalidateCursorRectsForView:[self.window contentView]];
        printf("cursor rect set\n");
    } else if (strncmp(line, "screens", 7) == 0) {
        for (NSScreen *screen in [NSScreen screens]) {
            NSRect f = [screen frame], v = [screen visibleFrame];
            printf("screen frame=%.0f,%.0f,%.0f,%.0f visible=%.0f,%.0f,%.0f,%.0f\n",
                f.origin.x, f.origin.y, f.size.width, f.size.height,
                v.origin.x, v.origin.y, v.size.width, v.size.height);
        }
        printf("menu bar height=%.0f status bar thickness=%.0f\n",
            [[NSApp mainMenu] menuBarHeight], [[NSStatusBar systemStatusBar] thickness]);
    }
    else if (strncmp(line, "menus", 5) == 0)
        [self addTestMenu];
    else if (strncmp(line, "tags", 4) == 0) {
        for (NSMenuItem *item in [[[[NSApp mainMenu] itemWithTitle:@"Test"] submenu] itemArray])
            printf("tag %s %ld\n", [[item title] UTF8String], (long)[item tag]);
    } else if (strncmp(line, "keyequivs", 9) == 0)
        [self printKeyEquivalents];
    else if (strncmp(line, "context", 7) == 0)
        [self addContextMenu];
    else if (strncmp(line, "popupbutton", 11) == 0)
        [self addPopUpButton];
    else if (strncmp(line, "refusequit", 10) == 0) {
        self.refusesToQuit = YES;
        printf("refusing to quit\n");
    } else if (strncmp(line, "statusitem", 10) == 0)
        [self addStatusItem];
    else if (strncmp(line, "nostatusitem", 12) == 0) {
        [[NSStatusBar systemStatusBar] removeStatusItem:self.statusItem];
        self.statusItem = nil;
        printf("status item removed\n");
    }
    else if (strncmp(line, "popup", 5) == 0) {
        NSView *view = [self.window contentView];
        NSRect b = [view bounds];
        BOOL chosen = [self.contextMenu popUpMenuPositioningItem:[self.contextMenu itemAtIndex:1]
            atLocation:NSMakePoint(NSMidX(b), NSMidY(b)) inView:view];
        printf("popup chosen=%d\n", chosen);
    }
    else if (sscanf(line, "activate %d", &pid) == 1) {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
        BOOL ok = [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        printf("activate %d: %s\n", pid, ok ? "YES" : "NO");
    }
    if (strncmp(line, "quit", 4) == 0)
        exit(0);
}

@end

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        DemoDelegate *delegate = [DemoDelegate new];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
