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

    /*
     * -two: a red window over the blue one, and commands, one at a time,
     * from /tmp/ws-demo.cmd (deleted when done): "raise-blue", "move-red X
     * Y" (its frame's origin), "quit". For the window server's tests while
     * there is no input to click with.
     */
    if (![[[NSProcessInfo processInfo] arguments] containsObject:@"-two"])
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

- (void)secondMoved:(NSNotification *)note {
    NSRect f = [self.second frame];
    printf("second moved frame=%.0f,%.0f,%.0f,%.0f\n", f.origin.x, f.origin.y,
        f.size.width, f.size.height);
}

- (void)readCommand:(NSTimer *)timer {
    const char *path = "/tmp/ws-demo.cmd";
    char line[64] = "";
    double x, y;
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return;
    if (fgets(line, sizeof(line), f) == NULL)
        line[0] = '\0';
    fclose(f);
    if (strncmp(line, "raise-blue", 10) == 0)
        [self.window orderFront:nil];
    else if (sscanf(line, "move-red %lf %lf", &x, &y) == 2)
        [self.second setFrameOrigin:NSMakePoint(x, y)];
    unlink(path);
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
