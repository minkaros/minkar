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

#import <AppKit/NSStatusBar.h>
#import <AppKit/NSStatusItem.h>
#import <CoreGraphics/CGDirectDisplay.h>

#import <AppKit/NSWorkspace.h>
#import <AppKit/NSRunningApplication.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSDistributedNotificationCenter.h>

@interface NSStatusItem (NSStatusBar)
- (instancetype)_initWithStatusBar:(NSStatusBar *)bar length:(CGFloat)length;
- (void)_remove;
- (void)_setHiddenByPresentation:(BOOL)hidden;
@end

@interface NSStatusBar (NSStatusBarPresentation)
- (NSArray *)_statusItems;
@end

/* The status items leave the menu bar with it: while the front app's
   presentation options hide or auto-hide the bar
   (NSApplication's setPresentationOptions:, a distributed notification). */
@interface _NSStatusBarPresentationWatcher : NSObject {
@public
    NSStatusBar *_bar;
    NSMutableDictionary *_options;   /* pid to options */
    pid_t _front;
}
@end

@implementation _NSStatusBarPresentationWatcher

- (void)apply {
    NSUInteger options = [[_options objectForKey:[NSNumber numberWithInt:_front]] unsignedIntegerValue];
    BOOL hidden = (options & (NSApplicationPresentationHideMenuBar | NSApplicationPresentationAutoHideMenuBar)) != 0;

    for(NSStatusItem *item in [_bar _statusItems])
        [item _setHiddenByPresentation:hidden];
}

- (void)optionsChanged:(NSNotification *)note {
    pid_t pid = [[note object] intValue];
    NSNumber *options = [[note userInfo] objectForKey:@"options"];

    if(pid <= 0 || options == nil)
        return;
    [_options setObject:options forKey:[NSNumber numberWithInt:pid]];
    [self apply];
}

- (void)frontChanged:(NSNotification *)note {
    NSRunningApplication *app = [[note userInfo] objectForKey:NSWorkspaceApplicationKey];

    if([[note name] isEqualToString:NSWorkspaceDidTerminateApplicationNotification]) {
        [_options removeObjectForKey:[NSNumber numberWithInt:[app processIdentifier]]];
        if([app processIdentifier] == _front)
            _front = 0;
    }
    else if([app activationPolicy] == NSApplicationActivationPolicyRegular)
        _front = [app processIdentifier];
    [self apply];
}

@end

@implementation NSStatusBar

+ (NSStatusBar *)systemStatusBar {
    static NSStatusBar *shared;

    if(shared == nil)
        shared = [[NSStatusBar alloc] init];
    return shared;
}

- (instancetype)init {
    if((self = [super init]) != nil) {
        _statusItems = [[NSMutableArray alloc] init];
        /* Watched for the life of the bar (the system bar is never freed). */
        _NSStatusBarPresentationWatcher *watcher = [[_NSStatusBarPresentationWatcher alloc] init];
        NSNotificationCenter *workspace = [[NSWorkspace sharedWorkspace] notificationCenter];

        watcher->_bar = self;
        watcher->_options = [[NSMutableDictionary alloc] init];
        [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(optionsChanged:)
            name:@"org.minkar.presentationOptionsChanged" object:nil];
        [workspace addObserver:watcher selector:@selector(frontChanged:) name:NSWorkspaceDidActivateApplicationNotification object:nil];
        [workspace addObserver:watcher selector:@selector(frontChanged:) name:NSWorkspaceDidTerminateApplicationNotification object:nil];
    }
    return self;
}

- (BOOL)isVertical {
    return NO;
}

- (NSStatusItem *)statusItemWithLength:(CGFloat)length {
    NSStatusItem *item = [[NSStatusItem alloc] _initWithStatusBar:self length:length];

    [_statusItems addObject:item];
    return [item autorelease];
}

/* The app's items, for the place every one keeps after a Command-drag. */
- (NSArray *)_statusItems {
    return [[_statusItems copy] autorelease];
}

- (void)removeStatusItem:(NSStatusItem *)item {
    [item _remove];
    [_statusItems removeObjectIdenticalTo:item];
}

/* Status items sit in the menu bar, as tall as it. */
- (CGFloat)thickness {
    extern CGFloat _NSMenuBarHeightForDisplay(CGDirectDisplayID display);

    return _NSMenuBarHeightForDisplay(CGMainDisplayID());
}

@end
