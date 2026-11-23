/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>

@class NSString, NSWindow;

/* The windows shown as the tabs of one window, one selected (API 23). */
@interface NSWindowTabGroup : NSObject
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSArray<NSWindow *> *windows;
@property (getter=isOverviewVisible) BOOL overviewVisible;
@property (readonly, getter=isTabBarVisible) BOOL tabBarVisible;
@property (assign) NSWindow *selectedWindow;
- (void)addWindow:(NSWindow *)window;
- (void)insertWindow:(NSWindow *)window atIndex:(NSInteger)index;
- (void)removeWindow:(NSWindow *)window;
@end
