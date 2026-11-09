/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTitlebarAccessoryViewController (API 19): a view shown with the
 * window's title bar: in a band below it (and the toolbar), or at its
 * left or right.
 */

#import <AppKit/NSViewController.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSWindow.h>

@interface NSTitlebarAccessoryViewController : NSViewController {
    NSLayoutAttribute _layoutAttribute;
    CGFloat _fullScreenMinHeight;
    BOOL _hidden;
    BOOL _automaticallyAdjustsSize;
}

/* NSLayoutAttributeBottom (the default), Left or Right. */
@property NSLayoutAttribute layoutAttribute;
@property CGFloat fullScreenMinHeight;
@property (getter=isHidden) BOOL hidden;
@property BOOL automaticallyAdjustsSize;

@end

@interface NSWindow (NSTitlebarAccessories)
@property (copy) NSArray<__kindof NSTitlebarAccessoryViewController *> *titlebarAccessoryViewControllers;
- (void)addTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)childViewController;
- (void)insertTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)childViewController atIndex:(NSInteger)index;
- (void)removeTitlebarAccessoryViewControllerAtIndex:(NSInteger)index;
@end
