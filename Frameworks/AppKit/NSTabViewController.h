/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTabViewController (API 19): a tab view whose items are child view
 * controllers' views, chosen with a segmented strip or the window's
 * toolbar.
 */

#import <AppKit/NSViewController.h>
#import <AppKit/NSTabView.h>
#import <AppKit/NSTabViewItem.h>
#import <AppKit/NSToolbar.h>

typedef NS_ENUM(NSInteger, NSTabViewControllerTabStyle) {
    NSTabViewControllerTabStyleSegmentedControlOnTop = 0,
    NSTabViewControllerTabStyleSegmentedControlOnBottom,
    NSTabViewControllerTabStyleToolbar,
    NSTabViewControllerTabStyleUnspecified = -1,
};

@interface NSTabViewController : NSViewController <NSTabViewDelegate, NSToolbarDelegate> {
    NSTabView *_tabView;
    NSTabViewControllerTabStyle _tabStyle;
    NSViewControllerTransitionOptions _transitionOptions;
    BOOL _canPropagateSelectedChildViewControllerTitle;
    NSInteger _selectedTabViewItemIndex;
    NSToolbar *_toolbar;
}

@property NSTabViewControllerTabStyle tabStyle;
@property (strong) NSTabView *tabView;
@property NSViewControllerTransitionOptions transitionOptions;
@property BOOL canPropagateSelectedChildViewControllerTitle;
@property (copy) NSArray<NSTabViewItem *> *tabViewItems;
@property NSInteger selectedTabViewItemIndex;

- (void)addTabViewItem:(NSTabViewItem *)tabViewItem;
- (void)insertTabViewItem:(NSTabViewItem *)tabViewItem atIndex:(NSInteger)index;
- (void)removeTabViewItem:(NSTabViewItem *)tabViewItem;
- (NSTabViewItem *)tabViewItemForViewController:(NSViewController *)viewController;

@end
