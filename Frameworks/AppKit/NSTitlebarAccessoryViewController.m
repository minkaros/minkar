/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTitlebarAccessoryViewController.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSThemeFrame.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSLayoutAnchor.h>
#import <AppKit/NSLayoutGuide.h>
#import <objc/runtime.h>

@implementation NSTitlebarAccessoryViewController

-initWithNibName:(NSString *)name bundle:(NSBundle *)bundle {
   self=[super initWithNibName:name bundle:bundle];
   if(self!=nil){
    _layoutAttribute=NSLayoutAttributeBottom;
    _automaticallyAdjustsSize=YES;
   }
   return self;
}

-initWithCoder:(NSCoder *)coder {
   self=[super initWithCoder:coder];
   if(self!=nil){
    _layoutAttribute=NSLayoutAttributeBottom;
    _automaticallyAdjustsSize=YES;
   }
   return self;
}

static char containerKey;

/* What the window's frame places: the controller's view inside it, held
 * to its edges by constraints when the view uses them, by its autoresizing
 * mask otherwise. A view laid out by constraints and put straight in the
 * frame was moved by the layout engine to its own fitting size. */
-(NSView *)_titlebarContainerView {
   NSView *container=objc_getAssociatedObject(self,&containerKey);

   if(container==nil){
    container=[[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
    objc_setAssociatedObject(self,&containerKey,container,OBJC_ASSOCIATION_RETAIN);
   }
   return container;
}

-(void)_attachViewToContainer {
   NSView *view=[self view];
   NSView *container=[self _titlebarContainerView];
   NSSize  size=[view frame].size;

   if(view==nil || [view superview]==container)
    return;
   if(size.height<=0)
    size=[view fittingSize];
   [container setFrameSize:size];
   [container addSubview:view];
   if([view translatesAutoresizingMaskIntoConstraints]){
    [view setFrame:[container bounds]];
    [view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
   }
   else {
    [NSLayoutConstraint activateConstraints:[NSArray arrayWithObjects:
      [[view leadingAnchor] constraintEqualToAnchor:[container leadingAnchor]],
      [[view trailingAnchor] constraintEqualToAnchor:[container trailingAnchor]],
      [[view topAnchor] constraintEqualToAnchor:[container topAnchor]],
      [[view bottomAnchor] constraintEqualToAnchor:[container bottomAnchor]],nil]];
   }
}

-(void)_retileWindow {
   NSView *frame=[[self _titlebarContainerView] superview];

   [frame resizeSubviewsWithOldSize:[frame bounds].size];
}

-(NSLayoutAttribute)layoutAttribute { return _layoutAttribute; }

-(void)setLayoutAttribute:(NSLayoutAttribute)attribute {
   _layoutAttribute=attribute;
   [self _retileWindow];
}

-(CGFloat)fullScreenMinHeight { return _fullScreenMinHeight; }
-(void)setFullScreenMinHeight:(CGFloat)value { _fullScreenMinHeight=value; }
-(BOOL)isHidden { return _hidden; }

-(void)setHidden:(BOOL)flag {
   _hidden=flag;
   [self _retileWindow];
}

-(BOOL)automaticallyAdjustsSize { return _automaticallyAdjustsSize; }
-(void)setAutomaticallyAdjustsSize:(BOOL)flag { _automaticallyAdjustsSize=flag; }

@end

static char accessoriesKey;

@implementation NSWindow (NSTitlebarAccessories)

-(NSMutableArray *)_accessories {
   NSMutableArray *accessories=objc_getAssociatedObject(self,&accessoriesKey);

   if(accessories==nil){
    accessories=[NSMutableArray array];
    objc_setAssociatedObject(self,&accessoriesKey,accessories,OBJC_ASSOCIATION_RETAIN);
   }
   return accessories;
}

-(NSArray *)titlebarAccessoryViewControllers {
   return [[[self _accessories] copy] autorelease];
}

-(void)setTitlebarAccessoryViewControllers:(NSArray *)controllers {
   while([[self _accessories] count]>0)
    [self removeTitlebarAccessoryViewControllerAtIndex:0];
   for(NSTitlebarAccessoryViewController *controller in controllers)
    [self addTitlebarAccessoryViewController:controller];
}

-(void)_tileTitlebar {
   NSView *frame=[self _backgroundView];

   [frame resizeSubviewsWithOldSize:[frame bounds].size];
   [frame setNeedsDisplay:YES];
}

-(void)addTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller {
   [self insertTitlebarAccessoryViewController:controller atIndex:[[self _accessories] count]];
}

-(void)insertTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller atIndex:(NSInteger)index {
   if(controller==nil)
    return;
   [[self _accessories] insertObject:controller atIndex:index];
   [controller _attachViewToContainer];
   [[self _backgroundView] addSubview:[controller _titlebarContainerView]];
   [self _tileTitlebar];
}

-(void)removeTitlebarAccessoryViewControllerAtIndex:(NSInteger)index {
   NSTitlebarAccessoryViewController *controller=[[[[self _accessories] objectAtIndex:index] retain] autorelease];

   [[controller _titlebarContainerView] removeFromSuperview];
   [[self _accessories] removeObjectAtIndex:index];
   [self _tileTitlebar];
}

@end
