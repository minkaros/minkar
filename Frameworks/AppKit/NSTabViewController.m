/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTabViewController.h>
#import <AppKit/NSToolbarItem.h>
#import <AppKit/NSWindow.h>

@implementation NSTabViewController

-initWithNibName:(NSString *)name bundle:(NSBundle *)bundle {
   self=[super initWithNibName:name bundle:bundle];
   if(self!=nil){
    _transitionOptions=NSViewControllerTransitionCrossfade|NSViewControllerTransitionAllowUserInteraction;
    _canPropagateSelectedChildViewControllerTitle=YES;
    _selectedTabViewItemIndex=-1;
   }
   return self;
}

-(void)dealloc {
   [_tabView setDelegate:nil];
   [_tabView release];
   [_toolbar setDelegate:nil];
   [_toolbar release];
   [super dealloc];
}

-(NSTabView *)tabView {
   if(_tabView==nil){
    _tabView=[[NSTabView alloc] initWithFrame:NSMakeRect(0,0,480,270)];
    [self _applyTabStyle];
   }
   return _tabView;
}

-(void)setTabView:(NSTabView *)tabView {
   tabView=[tabView retain];
   [_tabView release];
   _tabView=tabView;
}

/* A segmented strip on the tab view's top or bottom edge, or no tabs
   (the window's toolbar chooses, or the app does). */
-(void)_applyTabStyle {
   switch(_tabStyle){
    case NSTabViewControllerTabStyleSegmentedControlOnTop:
     [_tabView setTabViewType:NSTopTabsBezelBorder];
     break;
    case NSTabViewControllerTabStyleSegmentedControlOnBottom:
     [_tabView setTabViewType:NSBottomTabsBezelBorder];
     break;
    default:
     [_tabView setTabViewType:NSNoTabsNoBorder];
     break;
   }
}

-(NSTabViewControllerTabStyle)tabStyle {
   return _tabStyle;
}

-(void)setTabStyle:(NSTabViewControllerTabStyle)style {
   _tabStyle=style;
   if(_tabView!=nil)
    [self _applyTabStyle];
   [self _updateToolbar];
}

-(NSViewControllerTransitionOptions)transitionOptions { return _transitionOptions; }
-(void)setTransitionOptions:(NSViewControllerTransitionOptions)options { _transitionOptions=options; }
-(BOOL)canPropagateSelectedChildViewControllerTitle { return _canPropagateSelectedChildViewControllerTitle; }
-(void)setCanPropagateSelectedChildViewControllerTitle:(BOOL)flag { _canPropagateSelectedChildViewControllerTitle=flag; }

-(void)loadView {
   if([self nibName]!=nil){
    [super loadView];
    return;
   }
   [self setView:[self tabView]];
}

-(void)viewDidLoad {
   [super viewDidLoad];
   [[self tabView] setDelegate:self];
   if(_selectedTabViewItemIndex<0 && [[self tabViewItems] count]>0)
    [self setSelectedTabViewItemIndex:0];
}

/* The view is in its window now: the toolbar style can use it. */
-(void)viewDidAppear {
   [super viewDidAppear];
   [self _updateToolbar];
}

-(NSArray *)tabViewItems {
   return [[self tabView] tabViewItems];
}

-(void)setTabViewItems:(NSArray *)items {
   for(NSTabViewItem *item in [[[self tabViewItems] copy] autorelease])
    [self removeTabViewItem:item];
   for(NSTabViewItem *item in items)
    [self addTabViewItem:item];
}

-(void)addTabViewItem:(NSTabViewItem *)item {
   [self insertTabViewItem:item atIndex:[[self tabViewItems] count]];
}

-(void)insertTabViewItem:(NSTabViewItem *)item atIndex:(NSInteger)index {
   if([item viewController]!=nil)
    [self insertChildViewController:[item viewController] atIndex:index];
   [[self tabView] insertTabViewItem:item atIndex:index];
   if(_selectedTabViewItemIndex<0)
    [self setSelectedTabViewItemIndex:0];
   [self _updateToolbar];
}

-(void)removeTabViewItem:(NSTabViewItem *)item {
   [[item retain] autorelease];
   [[self tabView] removeTabViewItem:item];
   [[item viewController] removeFromParentViewController];
   if(_selectedTabViewItemIndex>=(NSInteger)[[self tabViewItems] count])
    [self setSelectedTabViewItemIndex:(NSInteger)[[self tabViewItems] count]-1];
   [self _updateToolbar];
}

-(NSTabViewItem *)tabViewItemForViewController:(NSViewController *)viewController {
   for(NSTabViewItem *item in [self tabViewItems])
    if([item viewController]==viewController)
     return item;
   return nil;
}

-(NSInteger)selectedTabViewItemIndex {
   return _selectedTabViewItemIndex;
}

-(void)setSelectedTabViewItemIndex:(NSInteger)index {
   NSArray *items=[self tabViewItems];

   if(index<0 || index>=(NSInteger)[items count]){
    _selectedTabViewItemIndex=-1;
    return;
   }
   if([[self tabView] indexOfTabViewItem:[[self tabView] selectedTabViewItem]]!=index)
    [[self tabView] selectTabViewItemAtIndex:index];
   [self _noteSelected:[items objectAtIndex:index]];
}

-(void)_noteSelected:(NSTabViewItem *)item {
   NSInteger index=[[self tabView] indexOfTabViewItem:item];

   if(index!=_selectedTabViewItemIndex){
    [self willChangeValueForKey:@"selectedTabViewItemIndex"];
    _selectedTabViewItemIndex=index;
    [self didChangeValueForKey:@"selectedTabViewItemIndex"];
   }
   if(_canPropagateSelectedChildViewControllerTitle && [[item viewController] title]!=nil)
    [self setTitle:[[item viewController] title]];
   if(_toolbar!=nil && index>=0)
    [_toolbar setSelectedItemIdentifier:[self _toolbarIdentifierAtIndex:index]];
}

#pragma mark NSTabViewDelegate

-(BOOL)tabView:(NSTabView *)tabView shouldSelectTabViewItem:(NSTabViewItem *)item {
   return YES;
}

-(void)tabView:(NSTabView *)tabView willSelectTabViewItem:(NSTabViewItem *)item {
}

-(void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)item {
   [self _noteSelected:item];
}

-(void)tabViewDidChangeNumberOfTabViewItems:(NSTabView *)tabView {
}

#pragma mark The toolbar style

-(NSString *)_toolbarIdentifierAtIndex:(NSInteger)index {
   return [NSString stringWithFormat:@"NSTabViewController.%ld",(long)index];
}

/* With the toolbar style, the window's toolbar has a selectable item for
   each tab. */
-(void)_updateToolbar {
   NSWindow *window=[self isViewLoaded]?[[self view] window]:nil;
   NSInteger i,count;

   if(_tabStyle!=NSTabViewControllerTabStyleToolbar || window==nil)
    return;
   if(_toolbar==nil){
    _toolbar=[[NSToolbar alloc] initWithIdentifier:[NSString stringWithFormat:@"NSTabViewController.%p",self]];
    [_toolbar setDelegate:self];
    [_toolbar setAllowsUserCustomization:NO];
    [_toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
   }
   if([window toolbar]!=_toolbar)
    [window setToolbar:_toolbar];
   else {
    while([[_toolbar items] count]>0)
     [_toolbar removeItemAtIndex:0];
    count=[[self tabViewItems] count];
    for(i=0;i<count;i++)
     [_toolbar insertItemWithItemIdentifier:[self _toolbarIdentifierAtIndex:i] atIndex:i];
   }
   if(_selectedTabViewItemIndex>=0)
    [_toolbar setSelectedItemIdentifier:[self _toolbarIdentifierAtIndex:_selectedTabViewItemIndex]];
}

-(NSArray *)_toolbarIdentifiers {
   NSMutableArray *result=[NSMutableArray array];
   NSInteger       i,count=[[self tabViewItems] count];

   for(i=0;i<count;i++)
    [result addObject:[self _toolbarIdentifierAtIndex:i]];
   return result;
}

-(NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
   return [self _toolbarIdentifiers];
}

-(NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
   return [self _toolbarIdentifiers];
}

-(NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
   return [self _toolbarIdentifiers];
}

-(NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)identifier willBeInsertedIntoToolbar:(BOOL)flag {
   NSInteger      index=[[self _toolbarIdentifiers] indexOfObject:identifier];
   NSTabViewItem *item;
   NSToolbarItem *result;

   if(index==NSNotFound)
    return nil;
   item=[[self tabViewItems] objectAtIndex:index];
   result=[[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
   [result setLabel:([item label]!=nil)?[item label]:@""];
   [result setPaletteLabel:[result label]];
   [result setImage:[item image]];
   [result setToolTip:[item toolTip]];
   [result setTag:index];
   [result setTarget:self];
   [result setAction:@selector(_selectTabFromToolbarItem:)];
   return result;
}

-(void)_selectTabFromToolbarItem:(NSToolbarItem *)sender {
   [self setSelectedTabViewItemIndex:[sender tag]];
}

@end
