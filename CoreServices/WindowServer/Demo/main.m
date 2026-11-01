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
@end

@implementation DemoView

- (BOOL)acceptsFirstResponder {
    return YES;
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

@interface DemoDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSWindow *second;
@property (copy) NSString *commandPath;
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
     * "minimize" and "restore" (its window).
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
