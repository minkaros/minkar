/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock (Dock-Argos.md, D1): the desktop on every display and the Dock
 * on the main one. An agent app (LSUIElement): no menu bar of its own.
 */

#import "DesktopWindow.h"
#import "DockView.h"
#include <notify.h>
#include <unistd.h>

@interface DockController : NSObject <NSApplicationDelegate>
@end

@implementation DockController {
    DockModel *_model;
    DockWindow *_dock;
    NSMutableArray<DesktopWindow *> *_desktops;
}

- (void)showDesktops
{
    for (DesktopWindow *window in _desktops)
        [window orderOut:nil];
    _desktops = [NSMutableArray array];
    for (NSScreen *screen in [NSScreen screens]) {
        DesktopWindow *window = [[DesktopWindow alloc] initWithScreen:screen];
        [window orderFront:nil];
        [_desktops addObject:window];
    }
}

/* NSWorkspace posts org.minkar.desktop.changed when a picture is set. */
- (void)watchDesktopPictures
{
    int fd = -1, token;

    if (notify_register_file_descriptor("org.minkar.desktop.changed", &fd, 0, &token) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(fd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(desktopPicturesChanged) withObject:nil waitUntilDone:NO];
    }];
}

- (void)desktopPicturesChanged
{
    for (DesktopWindow *window in _desktops)
        [window reload];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self showDesktops];
    [self watchDesktopPictures];
    _model = [[DockModel alloc] init];
    _dock = [[DockWindow alloc] initWithModel:_model];
    [_dock orderFront:nil];
}

- (void)applicationDidChangeScreenParameters:(NSNotification *)note
{
    [self showDesktops];
    [_dock layout];
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        DockController *controller = [[DockController alloc] init];

        [app setDelegate:controller];
        [app run];
    }
    return 0;
}
