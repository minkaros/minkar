/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#import "NSMenuBarView.h"
#import "NSMenuWindow.h"
#import <AppKit/NSFont.h>
#import <AppKit/NSGraphicsStyle.h>
@implementation NSMenuBarView

-(BOOL)_isMenuBar {
    return YES;
}

+(NSFont *)menuFont {
    return [NSFont menuFontOfSize:15.0];
}

+(float)menuHeight {
    NSDictionary *attributes=[NSDictionary dictionaryWithObjectsAndKeys:
        [self menuFont], NSFontAttributeName, nil];
    float result=[@"Menu" sizeWithAttributes:attributes].height;

    result+=4;
    return result;
}

-(Margins)menuItemTextMargins {
    Margins result = {0};
    result.top = 2;
    result.bottom = 2;
    return result;
}

-initWithFrame:(NSRect)frame menu:(NSMenu *)menu {
    self = [super initWithFrame:frame];
    _menu=menu;
    _font=[[self class] menuFont];
    _selectedItemIndex=NSNotFound;
    [self sizeToFit];
    return self;
}

-(BOOL)isFlipped {
   return YES;
}

-(BOOL)isOpaque {
   return YES;
}

-(NSMenu *)menu {
   return _menu;
}

-(void)setMenu:(NSMenu *)menu {
   [super setMenu:menu];
   [self sizeToFit];
}

-(NSRect)titleRectForItem:(NSMenuItem *)item previousBorderRect:(NSRect)previousBorderRect
{
    NSRect result;
    NSSize titleSize = NSZeroSize;
    if([item hasSubmenu] && [[[item submenu] _name] isEqualToString:@"NSAppleMenu"])
        titleSize = [[self graphicsStyle] menuItemAttributedTextSize:[item attributedTitle]];
    else
        titleSize = [[self graphicsStyle] menuItemTextSize:[item title]];
    NSImage *img = [item image];
    if(img) {
        titleSize.width += [img size].width;
        titleSize.height = MAX(titleSize.height, kNSMenuItemHeight);
    }

    result.origin = NSMakePoint(NSMaxX(previousBorderRect)+6,floor(([self bounds].size.height-titleSize.height)/2));
    result.size = titleSize;

    return result;
}

-(NSRect)borderRectFromTitleRect:(NSRect)titleRect {
    NSRect result=NSInsetRect(titleRect,-6,0);

    result.size.height=[self bounds].size.height;
    result.origin.y=0;

    return result;
}

-(void)sizeToFit {
    [self setFrameSize:NSMakeSize([self frame].size.width,[[self class] menuHeight])];
}

/* The leading item (the system menu, NSMenuBarWindow) and then the
 * menu's: the app's items stay in its own menu. */
-(NSArray *)itemArray {
   NSArray *items=[[self menu] itemArray];

   if(_leadingItem==nil)
    return items;
   return [[NSArray arrayWithObject:_leadingItem] arrayByAddingObjectsFromArray:items];
}

-(void)setLeadingItem:(NSMenuItem *)item {
   [_leadingItem autorelease];
   _leadingItem=[item retain];
   [self setNeedsDisplay:YES];
}

-(BOOL)acceptsFirstMouse:(NSEvent *)event {
   return YES;
}

-(NSArray *)visibleItemArray
{
   NSArray * items = [self itemArray];
   
   // Construct a new array of just the visible items
   if( _visibleArray == NULL )
      _visibleArray = [[NSMutableArray alloc] init];
	
   [_visibleArray removeAllObjects];
   
   int i;
   for( i = 0; i < [items count]; i++ )
   {
      NSMenuItem *item = [items objectAtIndex: i];
	  if( ![item isHidden] )
	     [_visibleArray addObject: item];
   }
   
   return _visibleArray;
}

-(NSImage *)overflowImage {
   return [NSImage imageNamed:@"NSMenuViewDoubleRightArrow"];
}

-(NSRect)overflowRect {
   NSRect   bounds=[self bounds];
   NSImage *image=[self overflowImage];
   NSSize   size=[image size];
   NSRect   rect=NSInsetRect(NSMakeRect(0,0,size.width,bounds.size.height),-3,0);

   rect.origin.x=NSMaxX(bounds)-rect.size.width;

   return rect;
}

-(void)drawRect:(NSRect)rect {
	NSRect        bounds=[self bounds];
	NSArray      *items=[self itemArray];
	int           i,count=[items count];
	NSRect        previousBorderRect=NSMakeRect(0,0,0,0);
	BOOL          overflow=NO;
	NSPoint       mouseLoc = [[NSApp currentEvent] locationInWindow];
	    
	mouseLoc = [self convertPoint:mouseLoc fromView:nil];
	
	[[self graphicsStyle] drawMenuBarBackgroundInRect:bounds];
	
	for(i=0;i<count;i++){
		NSMenuItem *item=[items objectAtIndex:i];
		NSString   *title=[item title];
		NSRect      titleRect=[self titleRectForItem:item previousBorderRect:previousBorderRect];
		NSRect      borderRect=[self borderRectFromTitleRect:titleRect];
		
		[[self graphicsStyle] drawMenuBarItemBorderInRect:borderRect hover:(i==_selectedItemIndex)/*NSPointInRect(mouseLoc,borderRect)*/ selected:(i==_selectedItemIndex)];
		
		titleRect.origin.x = borderRect.origin.x + (NSWidth(borderRect) - NSWidth(titleRect)) / 2;
		titleRect.origin.y = borderRect.origin.y + (NSHeight(borderRect) - NSHeight(titleRect)) / 2;

                NSImage *img = [item image];
                if(img) {
                    NSPoint pt = titleRect.origin;
                    pt.y = (kNSMenuItemHeight / 2) + ([img size].height / 2);
                    [[item image] compositeToPoint:pt operation:NSCompositeSourceOver];
                }

                if([item hasSubmenu] && [[[item submenu] _name] isEqualToString:@"NSAppleMenu"]) 
                    [[self graphicsStyle] drawAttributedMenuBarItemText:[item attributedTitle]
                        inRect:titleRect enabled:YES selected:(i==_selectedItemIndex)];
                else
                    [[self graphicsStyle] drawMenuBarItemText:title inRect:titleRect
                        enabled:YES selected:(i==_selectedItemIndex)];
		
		previousBorderRect=borderRect;
		
		if(NSMaxX(borderRect)>NSMaxX(bounds)){
			overflow=YES;
			break;
		}
	}
	
	if(overflow){
		NSImage *image=[self overflowImage];
		NSSize   size=[image size];
		NSRect   rect=[self overflowRect];
		NSPoint  origin;
		NSRect   fill=rect;
		
		[[NSColor controlColor] setFill];
		fill.origin.x-=4;
		fill.size.width+=4;
		NSRectFill(fill);
		
#if 0
		if(_selectedItemIndex==count)
			drawSunkenBorder(rect);
#endif

		origin=rect.origin;
		origin.x+=3;
		origin.y+=floor((rect.size.height-size.height)/2);
		[image compositeToPoint:origin operation:NSCompositeSourceOver];
	}
}

-(unsigned)overflowIndex {
   NSRect        bounds=[self bounds];
   NSArray      *items=[self itemArray];
   unsigned      i,count=[items count];
   NSRect        previousBorderRect=NSMakeRect(0,0,0,0);

   for(i=0;i<count;i++){
    NSMenuItem *item=[items objectAtIndex:i];
    NSRect      titleRect=[self titleRectForItem:item previousBorderRect:previousBorderRect];
    NSRect      borderRect=[self borderRectFromTitleRect:titleRect];

    if(NSMaxX(borderRect)>NSMaxX(bounds))
     return i;

    previousBorderRect=borderRect;
   }

   return NSNotFound;
}

-(unsigned)itemIndexAtPoint:(NSPoint)point {
   NSInteger    result=NSNotFound;
   NSRect        bounds=[self bounds];
   NSArray      *items=[self itemArray];
   unsigned      i,count=[items count];
   NSRect        previousBorderRect=NSMakeRect(0,0,0,0);
   BOOL          overflow=NO;

   for(i=0;i<count;i++){
    NSMenuItem *item=[items objectAtIndex:i];
    NSRect      titleRect=[self titleRectForItem:item previousBorderRect:previousBorderRect];
    NSRect      borderRect=[self borderRectFromTitleRect:titleRect];

    if(NSMaxX(borderRect)>NSMaxX(bounds))
     overflow=YES;

    if(NSMouseInRect(point,borderRect,[self isFlipped]))
     result=i;

    previousBorderRect=borderRect;
   }

   if(overflow){
    if(NSMouseInRect(point,[self overflowRect],[self isFlipped]))
     return count;
   }

   return result;
}

-(void)positionBranchForSelectedItem:(NSMenuWindow *)branch screen:(NSScreen *)screen {
   NSRect        branchFrame=[branch frame];
   NSRect        screenVisible=[screen visibleFrame];
   NSArray      *items=[self itemArray];
   unsigned      i,count=[items count];
   NSRect        previousBorderRect=NSMakeRect(0,0,0,0);
   NSRect        itemRect=NSZeroRect;
   NSPoint       topLeft=NSZeroPoint;

   [[branch menuView] setWindow:branch]; // make sure something with ARC keeps a ref!

   if(_selectedItemIndex==count){
    itemRect=[self overflowRect];
    topLeft=NSMakePoint(itemRect.origin.x,NSMaxY(itemRect));
   } else {
    for(i=0;i<count;i++){
     NSMenuItem *item=[items objectAtIndex:i];
     NSRect      titleRect=[self titleRectForItem:item previousBorderRect:previousBorderRect];
     itemRect=[self borderRectFromTitleRect:titleRect];
 
     if(i==_selectedItemIndex){
      topLeft=NSMakePoint(itemRect.origin.x,NSMaxY(itemRect));
      break;
     }

     previousBorderRect=itemRect;
    }
   }

   topLeft=[self convertPoint:topLeft toView:nil];
   topLeft=[[self window] convertBaseToScreen:topLeft];

   if(topLeft.y-branchFrame.size.height<NSMinY(screenVisible)){
    topLeft=NSMakePoint(itemRect.origin.x,NSMinY(itemRect));

    topLeft=[self convertPoint:topLeft toView:nil];
    topLeft=[[self window] convertBaseToScreen:topLeft];

    topLeft.y+=branchFrame.size.height;
   }

   if(topLeft.x+branchFrame.size.width>NSMaxX(screenVisible))
    topLeft.x=NSMaxX(screenVisible)-branchFrame.size.width;
   if(topLeft.x<NSMinX(screenVisible))
    topLeft.x=NSMinX(screenVisible);

  [branch setFrameTopLeftPoint:topLeft];
}

-(NSMenuView *)viewAtSelectedIndexPositionOnScreen:(NSScreen *)screen {
   NSArray *items=[self visibleItemArray];

#if 0
   if(_selectedItemIndex==[items count]){
    NSMenuWindow *branch=[[NSMenuWindow alloc] initWithMenu:[self menu] overflowAtIndex:[self overflowIndex]];

    [self positionBranchForSelectedItem:branch screen:screen];

    [branch orderFront:nil];
    return [branch menuView];
   }
#endif
   if(_selectedItemIndex<[items count]){
    NSMenuItem *item=[items objectAtIndex:_selectedItemIndex];

    if([item hasSubmenu]){
     if([[[item submenu] itemArray] count]>0){
      NSMenuWindow *branch=[[NSMenuWindow alloc] initWithMenu:[item submenu]];

      [self positionBranchForSelectedItem:branch screen:screen];

      [branch orderFront:nil];
      return [branch menuView];
     }
    }
   }
   return nil;
}

-(NSMenuItem *)itemAtSelectedIndex {
   NSArray *items=[self visibleItemArray];

   if(_selectedItemIndex<[items count])
    return [items objectAtIndex:_selectedItemIndex];

   return nil;
}

@end
