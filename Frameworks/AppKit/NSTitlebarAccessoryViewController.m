/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTitlebarAccessoryViewController.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSThemeFrame.h>
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

-(NSLayoutAttribute)layoutAttribute { return _layoutAttribute; }

-(void)setLayoutAttribute:(NSLayoutAttribute)attribute {
   _layoutAttribute=attribute;
   [[[self view] superview] resizeSubviewsWithOldSize:[[[self view] superview] bounds].size];
}

-(CGFloat)fullScreenMinHeight { return _fullScreenMinHeight; }
-(void)setFullScreenMinHeight:(CGFloat)value { _fullScreenMinHeight=value; }
-(BOOL)isHidden { return _hidden; }

-(void)setHidden:(BOOL)flag {
   _hidden=flag;
   [[[self view] superview] resizeSubviewsWithOldSize:[[[self view] superview] bounds].size];
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
   [[self _backgroundView] addSubview:[controller view]];
   [self _tileTitlebar];
}

-(void)removeTitlebarAccessoryViewControllerAtIndex:(NSInteger)index {
   NSTitlebarAccessoryViewController *controller=[[[[self _accessories] objectAtIndex:index] retain] autorelease];

   [[controller view] removeFromSuperview];
   [[self _accessories] removeObjectAtIndex:index];
   [self _tileTitlebar];
}

@end
