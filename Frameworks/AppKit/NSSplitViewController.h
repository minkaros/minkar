/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSSplitViewController and NSSplitViewItem (API 19): a split view whose
 * panes are child view controllers' views.
 */

#import <AppKit/NSViewController.h>
#import <AppKit/NSSplitView.h>

APPKIT_EXPORT const CGFloat NSSplitViewItemUnspecifiedDimension;
APPKIT_EXPORT const CGFloat NSSplitViewControllerAutomaticDimension;

typedef NS_ENUM(NSInteger, NSSplitViewItemBehavior) {
    NSSplitViewItemBehaviorDefault = 0,
    NSSplitViewItemBehaviorSidebar = 1,
    NSSplitViewItemBehaviorContentList = 2,
};

typedef NS_ENUM(NSInteger, NSSplitViewItemCollapseBehavior) {
    NSSplitViewItemCollapseBehaviorDefault = 0,
    NSSplitViewItemCollapseBehaviorPreferResizingSplitViewWithFixedSiblings = 1,
    NSSplitViewItemCollapseBehaviorPreferResizingSiblingsWithFixedSplitView = 2,
    NSSplitViewItemCollapseBehaviorUseConstraints = 3,
};

@class NSSplitViewController;

@interface NSSplitViewItem : NSObject {
    NSViewController *_viewController;
    NSSplitViewController *_splitViewController;
    NSSplitViewItemBehavior _behavior;
    NSSplitViewItemCollapseBehavior _collapseBehavior;
    BOOL _collapsed;
    BOOL _canCollapse;
    BOOL _springLoaded;
    float _holdingPriority;
    CGFloat _minimumThickness;
    CGFloat _maximumThickness;
    CGFloat _preferredThicknessFraction;
    CGFloat _automaticMaximumThickness;
    CGFloat _expandedThickness;
}

+ (instancetype)splitViewItemWithViewController:(NSViewController *)viewController;
/* Collapsible, holding its width while the window resizes. */
+ (instancetype)sidebarWithViewController:(NSViewController *)viewController;
+ (instancetype)contentListWithViewController:(NSViewController *)viewController;

@property (strong) NSViewController *viewController;
@property (readonly) NSSplitViewItemBehavior behavior;
@property (getter=isCollapsed) BOOL collapsed;
@property BOOL canCollapse;
@property NSSplitViewItemCollapseBehavior collapseBehavior;
@property float holdingPriority;
@property CGFloat minimumThickness;
@property CGFloat maximumThickness;
@property CGFloat preferredThicknessFraction;
@property CGFloat automaticMaximumThickness;
@property (getter=isSpringLoaded) BOOL springLoaded;

@end

@interface NSSplitViewController : NSViewController <NSSplitViewDelegate> {
    NSSplitView *_splitView;
    NSMutableArray *_splitViewItems;
    CGFloat _minimumThicknessForInlineSidebars;
}

@property (strong) NSSplitView *splitView;
@property (copy) NSArray<NSSplitViewItem *> *splitViewItems;
@property CGFloat minimumThicknessForInlineSidebars;

- (void)addSplitViewItem:(NSSplitViewItem *)splitViewItem;
- (void)insertSplitViewItem:(NSSplitViewItem *)splitViewItem atIndex:(NSInteger)index;
- (void)removeSplitViewItem:(NSSplitViewItem *)splitViewItem;
- (NSSplitViewItem *)splitViewItemForViewController:(NSViewController *)viewController;
/* Collapses or expands the first sidebar. */
- (IBAction)toggleSidebar:(id)sender;

@end
