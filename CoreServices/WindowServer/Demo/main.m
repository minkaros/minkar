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

@interface DemoView : NSView
@end

@implementation DemoView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)drawRect:(NSRect)rect {
    [[NSColor colorWithCalibratedRed:0.1 green:0.3 blue:0.9 alpha:1] set];
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
