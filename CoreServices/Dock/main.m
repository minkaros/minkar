/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock (Atlas.md, D1): the desktop on every display, with an empty
 * menu bar under the apps' own, and the Dock on the main one. An agent app (LSUIElement): no menu bar of its own.
 */

#import "DesktopWindow.h"
#import "DockView.h"
#include <notify.h>
#include <objc/message.h>
#include <unistd.h>

@interface DockController : NSObject <NSApplicationDelegate>
@end

@implementation DockController {
    DockModel *_model;
    DockWindow *_dock;
    NSMutableArray<DesktopWindow *> *_desktops;
    NSMutableArray<NSWindow *> *_menuBarBackdrops;
    NSArray<NSValue *> *_desktopFrames;
}

/* The screens' frames the desktops were made for. */
- (NSArray<NSValue *> *)screenFrames
{
    NSMutableArray<NSValue *> *frames = [NSMutableArray array];

    for (NSScreen *screen in [NSScreen screens])
        [frames addObject:[NSValue valueWithRect:[screen frame]]];
    return frames;
}

/* A desktop for each screen; made again only when the screens changed:
   the visible part changing (the Dock grown by an app's tile) leaves them
   as they are, as each new full-screen window is a whole screen drawn
   again. */
- (void)showDesktops
{
    NSArray<NSValue *> *frames = [self screenFrames];

    if (_desktops != nil && [frames isEqualToArray:_desktopFrames])
        return;
    _desktopFrames = frames;
    for (DesktopWindow *window in _desktops)
        [window orderOut:nil];
    for (NSWindow *window in _menuBarBackdrops)
        [window orderOut:nil];
    _desktops = [NSMutableArray array];
    _menuBarBackdrops = [NSMutableArray array];
    for (NSScreen *screen in [NSScreen screens]) {
        DesktopWindow *window = [[DesktopWindow alloc] initWithScreen:screen];
        [window orderFront:nil];
        [_desktops addObject:window];
        /* An empty menu bar under the apps' own (AppKit's): while the
           front app changes the bar stays, instead of the desktop showing
           through for a moment. */
        Class bar = NSClassFromString(@"NSMenuBarWindow");
        SEL make = NSSelectorFromString(@"_backdropForScreen:");
        if ([bar respondsToSelector:make]) {
            NSWindow *backdrop = ((NSWindow * (*)(id, SEL, NSScreen *))objc_msgSend)(bar, make, screen);
            [backdrop orderFront:nil];
            if (backdrop != nil)
                [_menuBarBackdrops addObject:backdrop];
        }
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
