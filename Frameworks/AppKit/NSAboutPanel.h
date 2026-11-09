/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The standard About window (private to AppKit).
 */

#import <Foundation/NSObject.h>

@class NSDictionary, NSString, NSWindow;

@interface NSAboutPanel : NSObject {
    NSWindow *_window;
}

+ (NSAboutPanel *)sharedAboutPanel;
/* "Version 1.2 (34)", localized; the build left out when it is the same. */
+ (NSString *)versionStringWithApplicationVersion:(NSString *)applicationVersion version:(NSString *)version;
- (void)orderFrontWithOptions:(NSDictionary *)options;
- (NSWindow *)window;
@end
