/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSSplitViewController.h>

const CGFloat NSSplitViewItemUnspecifiedDimension=-1;
const CGFloat NSSplitViewControllerAutomaticDimension=-1;

@interface NSSplitViewItem (Minkar)
-(void)_setSplitViewController:(NSSplitViewController *)controller;
-(CGFloat)_expandedThickness;
-(void)_setExpandedThickness:(CGFloat)value;
-(void)_setCollapsedState:(BOOL)flag;
@end

@interface NSSplitViewController (Minkar)
-(void)_item:(NSSplitViewItem *)item didCollapse:(BOOL)collapsed;
@end

@implementation NSSplitViewItem

-init {
   _canCollapse=NO;
   _holdingPriority=250;
   _minimumThickness=NSSplitViewItemUnspecifiedDimension;
   _maximumThickness=NSSplitViewItemUnspecifiedDimension;
   _preferredThicknessFraction=NSSplitViewItemUnspecifiedDimension;
   _automaticMaximumThickness=NSSplitViewItemUnspecifiedDimension;
   return self;
}

+(instancetype)splitViewItemWithViewController:(NSViewController *)viewController {
   NSSplitViewItem *item=[[[self alloc] init] autorelease];

   [item setViewController:viewController];
   return item;
}

+(instancetype)sidebarWithViewController:(NSViewController *)viewController {
   NSSplitViewItem *item=[self splitViewItemWithViewController:viewController];

   item->_behavior=NSSplitViewItemBehaviorSidebar;
   item->_canCollapse=YES;
   item->_holdingPriority=260;
   item->_minimumThickness=140;
   return item;
}

+(instancetype)contentListWithViewController:(NSViewController *)viewController {
   NSSplitViewItem *item=[self splitViewItemWithViewController:viewController];

   item->_behavior=NSSplitViewItemBehaviorContentList;
   item->_holdingPriority=260;
   item->_minimumThickness=180;
   return item;
}

-(void)dealloc {
   [_viewController release];
   [super dealloc];
}

-(NSViewController *)viewController { return _viewController; }

-(void)setViewController:(NSViewController *)controller {
   controller=[controller retain];
   [_viewController release];
   _viewController=controller;
}

-(void)_setSplitViewController:(NSSplitViewController *)controller {
   _splitViewController=controller;
}

-(NSSplitViewItemBehavior)behavior { return _behavior; }
-(BOOL)isCollapsed { return _collapsed; }

/* A collapsed pane is hidden and keeps the thickness it had for when it
   expands again. */
-(void)setCollapsed:(BOOL)flag {
   if(_collapsed==flag)
    return;
   _collapsed=flag;
   [_splitViewController _item:self didCollapse:flag];
}

-(BOOL)canCollapse { return _canCollapse; }
-(void)setCanCollapse:(BOOL)flag { _canCollapse=flag; }
-(NSSplitViewItemCollapseBehavior)collapseBehavior { return _collapseBehavior; }
-(void)setCollapseBehavior:(NSSplitViewItemCollapseBehavior)value { _collapseBehavior=value; }
-(float)holdingPriority { return _holdingPriority; }

-(void)setHoldingPriority:(float)priority {
   _holdingPriority=priority;
   [_splitViewController _item:self didCollapse:_collapsed];
}

-(CGFloat)minimumThickness { return _minimumThickness; }
-(void)setMinimumThickness:(CGFloat)value { _minimumThickness=value; }
-(CGFloat)maximumThickness { return _maximumThickness; }
-(void)setMaximumThickness:(CGFloat)value { _maximumThickness=value; }
-(CGFloat)preferredThicknessFraction { return _preferredThicknessFraction; }
-(void)setPreferredThicknessFraction:(CGFloat)value { _preferredThicknessFraction=value; }
-(CGFloat)automaticMaximumThickness { return _automaticMaximumThickness; }
-(void)setAutomaticMaximumThickness:(CGFloat)value { _automaticMaximumThickness=value; }
-(BOOL)isSpringLoaded { return _springLoaded; }
-(void)setSpringLoaded:(BOOL)flag { _springLoaded=flag; }

-(void)_setCollapsedState:(BOOL)flag { _collapsed=flag; }
-(CGFloat)_expandedThickness { return _expandedThickness; }
-(void)_setExpandedThickness:(CGFloat)value { _expandedThickness=value; }

@end

@implementation NSSplitViewController

-initWithNibName:(NSString *)name bundle:(NSBundle *)bundle {
   self=[super initWithNibName:name bundle:bundle];
   if(self!=nil){
    _splitViewItems=[[NSMutableArray alloc] init];
    _minimumThicknessForInlineSidebars=NSSplitViewControllerAutomaticDimension;
   }
   return self;
}

-initWithCoder:(NSCoder *)coder {
   self=[super initWithCoder:coder];
   if(self!=nil)
    _splitViewItems=[[NSMutableArray alloc] init];
   return self;
}

-(void)dealloc {
   for(NSSplitViewItem *item in _splitViewItems)
    [item _setSplitViewController:nil];
   [_splitViewItems release];
   [_splitView release];
   [super dealloc];
}

-(NSSplitView *)splitView {
   if(_splitView==nil){
    _splitView=[[NSSplitView alloc] initWithFrame:NSMakeRect(0,0,480,270)];
    [_splitView setVertical:YES];
    [_splitView setDividerStyle:NSSplitViewDividerStyleThin];
   }
   return _splitView;
}

-(void)setSplitView:(NSSplitView *)splitView {
   splitView=[splitView retain];
   [_splitView release];
   _splitView=splitView;
}

/* The split view is the controller's view, unless a nib gave one. */
-(void)loadView {
   if([self nibName]!=nil){
    [super loadView];
    return;
   }
   [self setView:[self splitView]];
}

-(void)viewDidLoad {
   [super viewDidLoad];
   [[self splitView] setDelegate:self];
   [[self splitView] setArrangesAllSubviews:NO];
   for(NSSplitViewItem *item in _splitViewItems)
    [self _addPaneOfItem:item atIndex:[_splitViewItems indexOfObjectIdenticalTo:item]];
   [self _updatePriorities];
   [[self splitView] adjustSubviews];
}

-(NSArray *)splitViewItems {
   return [[_splitViewItems copy] autorelease];
}

-(void)setSplitViewItems:(NSArray *)items {
   while([_splitViewItems count]>0)
    [self removeSplitViewItem:[_splitViewItems lastObject]];
   for(NSSplitViewItem *item in items)
    [self addSplitViewItem:item];
}

-(CGFloat)minimumThicknessForInlineSidebars { return _minimumThicknessForInlineSidebars; }
-(void)setMinimumThicknessForInlineSidebars:(CGFloat)value { _minimumThicknessForInlineSidebars=value; }

-(void)_updatePriorities {
   NSInteger i,count=[_splitViewItems count];

   for(i=0;i<count;i++)
    [[self splitView] setHoldingPriority:[[_splitViewItems objectAtIndex:i] holdingPriority] forSubviewAtIndex:i];
}

-(void)_addPaneOfItem:(NSSplitViewItem *)item atIndex:(NSInteger)index {
   NSView *pane=[[item viewController] view];

   [pane setHidden:[item isCollapsed]];
   [[self splitView] insertArrangedSubview:pane atIndex:index];
}

-(void)addSplitViewItem:(NSSplitViewItem *)item {
   [self insertSplitViewItem:item atIndex:[_splitViewItems count]];
}

-(void)insertSplitViewItem:(NSSplitViewItem *)item atIndex:(NSInteger)index {
   [_splitViewItems insertObject:item atIndex:index];
   [item _setSplitViewController:self];
   [self insertChildViewController:[item viewController] atIndex:index];
   if([self isViewLoaded]){
    [self _addPaneOfItem:item atIndex:index];
    [self _updatePriorities];
    [[self splitView] adjustSubviews];
   }
}

-(void)removeSplitViewItem:(NSSplitViewItem *)item {
   NSUInteger index=[_splitViewItems indexOfObjectIdenticalTo:item];

   if(index==NSNotFound)
    return;
   [[item retain] autorelease];
   if([self isViewLoaded]){
    [[self splitView] removeArrangedSubview:[[item viewController] view]];
    [[[item viewController] view] removeFromSuperview];
   }
   [[item viewController] removeFromParentViewController];
   [item _setSplitViewController:nil];
   [_splitViewItems removeObjectAtIndex:index];
   if([self isViewLoaded]){
    [self _updatePriorities];
    [[self splitView] adjustSubviews];
   }
}

-(NSSplitViewItem *)splitViewItemForViewController:(NSViewController *)viewController {
   for(NSSplitViewItem *item in _splitViewItems)
    if([item viewController]==viewController)
     return item;
   return nil;
}

-(void)_item:(NSSplitViewItem *)item didCollapse:(BOOL)collapsed {
   NSView *pane;

   if(![self isViewLoaded])
    return;
   pane=[[item viewController] view];
   [self _updatePriorities];
   if(collapsed){
    NSRect frame=[pane frame];

    [item _setExpandedThickness:[[self splitView] isVertical]?NSWidth(frame):NSHeight(frame)];
    [pane setHidden:YES];
   }
   else if([pane isHidden]){
    NSRect  frame=[pane frame];
    CGFloat thickness=[item _expandedThickness];

    if(thickness<=0)
     thickness=MAX([item minimumThickness],100);
    if([[self splitView] isVertical])
     frame.size.width=thickness;
    else
     frame.size.height=thickness;
    [pane setFrame:frame];
    [pane setHidden:NO];
   }
   [[self splitView] adjustSubviews];
}

-(void)toggleSidebar:(id)sender {
   for(NSSplitViewItem *item in _splitViewItems)
    if([item behavior]==NSSplitViewItemBehaviorSidebar){
     [item setCollapsed:![item isCollapsed]];
     return;
    }
}

-(NSSplitViewItem *)_itemForPane:(NSView *)pane {
   for(NSSplitViewItem *item in _splitViewItems)
    if([[item viewController] isViewLoaded] && [[item viewController] view]==pane)
     return item;
   return nil;
}

#pragma mark NSSplitViewDelegate

-(BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview {
   return [[self _itemForPane:subview] canCollapse];
}

/* A divider stays where both panes keep their minimum and maximum
   thicknesses. */
-(CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)dividerIndex {
   NSArray *panes=[splitView arrangedSubviews];
   NSView  *before=[panes objectAtIndex:dividerIndex];
   CGFloat  minimum=[[self _itemForPane:before] minimumThickness];
   NSRect   frame=[before frame];

   if(minimum<=0)
    return proposed;
   return MAX(proposed,([splitView isVertical]?NSMinX(frame):NSMinY(frame))+minimum);
}

-(CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposed ofSubviewAt:(NSInteger)dividerIndex {
   NSArray *panes=[splitView arrangedSubviews];
   NSView  *before=[panes objectAtIndex:dividerIndex];
   NSView  *after=((NSUInteger)dividerIndex+1<[panes count])?[panes objectAtIndex:dividerIndex+1]:nil;
   CGFloat  result=proposed;
   CGFloat  maximum=[[self _itemForPane:before] maximumThickness];
   CGFloat  afterMinimum=[[self _itemForPane:after] minimumThickness];
   NSRect   frame=[before frame];

   if(maximum>0)
    result=MIN(result,([splitView isVertical]?NSMinX(frame):NSMinY(frame))+maximum);
   if(after!=nil && afterMinimum>0){
    NSRect afterFrame=[after frame];

    result=MIN(result,([splitView isVertical]?NSMaxX(afterFrame):NSMaxY(afterFrame))-afterMinimum-[splitView dividerThickness]);
   }
   return result;
}

-(BOOL)splitView:(NSSplitView *)splitView shouldHideDividerAtIndex:(NSInteger)dividerIndex {
   return NO;
}

/* The panes that collapse or expand through the divider tell their
   items. */
-(void)splitViewDidResizeSubviews:(NSNotification *)notification {
   for(NSSplitViewItem *item in _splitViewItems){
    NSView *pane=[[item viewController] isViewLoaded]?[[item viewController] view]:nil;

    if(pane!=nil && [pane isHidden]!=[item isCollapsed])
     [item _setCollapsedState:[pane isHidden]];
   }
}

@end
