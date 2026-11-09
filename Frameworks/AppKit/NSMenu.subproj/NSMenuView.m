/*
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * Contains code Copyright (c) 2006-2007 Christopher J. W. Lloyd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#import "NSMenuBarView.h"
#import "NSMenuWindow.h"

#define MIN_TITLE_KEY_GAP 8
#define WINDOW_BORDER_THICKNESS 1
#define IMAGE_TITLE_GAP 8

static NSRect boundsToTitleAreaRect(NSRect rect){
   return NSInsetRect(rect, WINDOW_BORDER_THICKNESS, WINDOW_BORDER_THICKNESS);
}

#import <AppKit/NSWindow-Private.h>
#include <SkyLight/SkyLight.h>

extern int _CGUsesSkyLight(void);

enum {
	kNSMenuKeyboardNavigationNone,
	kNSMenuKeyboardNavigationUp,
	kNSMenuKeyboardNavigationDown,
	kNSMenuKeyboardNavigationLeft,
	kNSMenuKeyboardNavigationRight,
	kNSMenuKeyboardNavigationLetter,
	kNSMenuKeyboardNavigationStart	// tracking begun from the keyboard
};

@implementation NSMenuView

/* The menu bar answers YES: its items run left to right. */
-(BOOL)_isMenuBar {
    return NO;
}

-(NSScreen *)_screenForPoint:(NSPoint)point {
   NSArray *screens=[NSScreen screens];
   int      i,count=[screens count];

   for(i=0;i<count;i++){
    NSScreen *check=[screens objectAtIndex:i];

    if(NSPointInRect(point,[check frame]))
     return check;
   }

   return [screens objectAtIndex:0];// should not happen
}


-initWithMenu:(NSMenu *)menu {
    NSRect frame = NSZeroRect;
    self = [self initWithFrame:frame];

    _menu = menu;
    _selectedItemIndex = NSNotFound;

    frame.size = [self sizeForMenuItems:[self visibleItemArray]];
    [self setFrame:frame];

    return self;
}

-(Margins)menuItemTextMargins {
    Margins result = {16};
    result.top = 4;
    result.bottom = 4;
    return result;
}

-(BOOL)isFlipped {
    return YES;
}

-(BOOL)isOpaque {
    return YES;
}

-(NSArray *)itemArray {
   return [[self menu] itemArray];
}

-(NSArray *)visibleItemArray {
    NSArray *items = [[self menu] itemArray];
   
    // Construct a new array of just the visible items
    if(_visibleArray == nil)
        _visibleArray = [NSMutableArray new];
    [_visibleArray removeAllObjects];
   
    for(int i = 0; i < [items count]; ++i) {
        NSMenuItem *item = [items objectAtIndex:i];
        if(![item isHidden])
	     [_visibleArray addObject:item];
    }

    return _visibleArray;
}

-(CGFloat)heightOfMenuItem:(NSMenuItem *)item {
    NSSize titleSize = [self contentSizeForItem:item];
    return titleSize.height;
}

#define ITEM_MAX(size) height = MAX(height, size.height); width += size.width;
-(NSSize)contentSizeForItem:(NSMenuItem *)item {
    if([item isSeparatorItem])	{
        return [[self graphicsStyle] menuItemSeparatorSize];
    } else {
        float width = [[self graphicsStyle] menuItemGutterGap];
        float height = 0.0f;

        NSSize size = [[self graphicsStyle] menuItemGutterSize];
        ITEM_MAX(size);

        size = [self sizeForMenuItemImage:item];
        if(size.width > 0.0f) {
            ITEM_MAX(size);
            width += IMAGE_TITLE_GAP;
        }

        size = [[self graphicsStyle] menuItemTextSize:[item title]];
        ITEM_MAX(size);

        if([[item keyEquivalent] length] == 0) {
            size = [[self graphicsStyle] menuItemTextSize:[item _keyEquivalentDescription]];
            ITEM_MAX(size);
        }

        if([item hasSubmenu] && [[item submenu] numberOfItems] > 0) {
            size = [[self graphicsStyle] menuItemBranchArrowSize];
            ITEM_MAX(size);
        }
		
        return NSMakeSize(width, MAX(height, kNSMenuItemHeight));
    }
}

-(NSSize)sizeForMenuItemImage:(NSMenuItem *)item {
    if([item image] != nil)
        return [[item image] size];
    return NSZeroSize;
}

-(NSSize)sizeForMenuItems:(NSArray *)items {
    NSSize result = NSZeroSize;
    float maxTitleWidth = 0;
    BOOL anItemHasAnImage = NO;
    float maxKeyWidth = 0; 
    float totalHeight = 0;
    NSSize gutterSize = [[self graphicsStyle] menuItemGutterSize];
    NSSize rightArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
    NSSize separatorSize = [[self graphicsStyle] menuItemSeparatorSize];
	
    for(int i = 0; i < [items count]; ++i) {
        NSMenuItem *item = [items objectAtIndex:i];
        if([item isSeparatorItem]) {
            totalHeight += separatorSize.height;
        } else {
            NSSize size = NSZeroSize;
            float height = 0;
            float titleAndIconWidth = 0;
			
            size = [self sizeForMenuItemImage:item];
            height = MAX(height, size.height);
            if ((titleAndIconWidth = size.width) > 0.0f)
                anItemHasAnImage = YES;

            size = [[self graphicsStyle] menuItemTextSize:[item title]];
            titleAndIconWidth += size.width;
            maxTitleWidth = MAX(maxTitleWidth, titleAndIconWidth);
            height = MAX(height, size.height);
            
            if([[item keyEquivalent] length] != 0)
            {
                size = [[self graphicsStyle] menuItemTextSize:[item _keyEquivalentDescription]];
                maxKeyWidth = MAX(maxKeyWidth, size.width);
                height = MAX(height, size.height);
            }
            height = MAX(height, gutterSize.height);
            height = MAX(height, rightArrowSize.height);
			
            totalHeight += height;
        }
    }
	
    result.height = totalHeight;
    result.width = gutterSize.width;
    result.width += [[self graphicsStyle] menuItemGutterGap];

    if(anItemHasAnImage)
        result.width += IMAGE_TITLE_GAP;

    result.width += maxTitleWidth;
    
    if(maxKeyWidth > 0.0f) {
        result.width += MIN_TITLE_KEY_GAP;
        result.width += maxKeyWidth;
    }

    result.width += rightArrowSize.width;
	
    // border
    result.width += WINDOW_BORDER_THICKNESS*2;
    result.height += WINDOW_BORDER_THICKNESS*2;
    
    return result;
}

-(CGFloat)_topOfItemAtIndex:(NSUInteger)index {
    NSArray *items = [self visibleItemArray];
    CGFloat y = WINDOW_BORDER_THICKNESS;

    for(NSUInteger i = 0; i < index && i < [items count]; i++)
        y += [self heightOfMenuItem:[items objectAtIndex:i]];
    return y;
}

/* A submenu opens to the right of this menu, its first item level with
   the selected one; to the left when it does not fit on the right, and
   moved up when it does not fit below. */
-(void)positionBranchForSelectedItem:(NSWindow *)branch screen:(NSScreen *)screen {
    NSRect branchFrame = [branch frame];
    NSRect screenVisible = [screen visibleFrame];
    NSArray *items = [self visibleItemArray];
    NSRect itemArea = boundsToTitleAreaRect([self bounds]);
    CGFloat y = itemArea.origin.y;
    NSUInteger i, count = [items count];

    for(i = 0; i < count && i < _selectedItemIndex; i++)
        y += [self heightOfMenuItem:[items objectAtIndex:i]];

    /* The view is flipped: the item's top is y, the branch's border
       above it. */
    NSPoint topLeft = NSMakePoint(NSMaxX([self bounds]), y - WINDOW_BORDER_THICKNESS);
    topLeft = [[self window] convertBaseToScreen:[self convertPoint:topLeft toView:nil]];

    if(topLeft.x + branchFrame.size.width > NSMaxX(screenVisible))
        topLeft.x = NSMinX([[self window] frame]) - branchFrame.size.width;
    if(topLeft.x < NSMinX(screenVisible))
        topLeft.x = NSMinX(screenVisible);
    if(topLeft.y - branchFrame.size.height < NSMinY(screenVisible))
        topLeft.y = NSMinY(screenVisible) + branchFrame.size.height;
    if(topLeft.y > NSMaxY(screenVisible))
        topLeft.y = NSMaxY(screenVisible);

    [branch setFrameTopLeftPoint:topLeft];
    [self setNeedsDisplay:YES];
}

-(NSMenuView *)viewAtSelectedIndexPositionOnScreen:(NSScreen *)screen {
    NSArray *items = [self visibleItemArray];

#if 0
    if(_selectedItemIndex==[items count]) {
        NSMenuWindow *branch=[[NSMenuWindow alloc] initWithMenu:[self menu] overflowAtIndex:[self overflowIndex]];

        [self positionBranchForSelectedItem:branch screen:screen];

        [branch orderFront:nil];
        return [branch menuView];
    }
#endif

    if(_selectedItemIndex < [items count]) {
        NSMenuItem *item = [items objectAtIndex:_selectedItemIndex];

        if([item hasSubmenu]) {
            if([[[item submenu] itemArray] count] > 0) {
                NSMenuWindow *branch = [[NSMenuWindow alloc] initWithMenu:[item submenu]];

                [self positionBranchForSelectedItem:branch screen:screen];

                [branch orderFront:nil];
                return [branch menuView];
            }
        }
    }
    return nil;
}

-(NSRect)titleRectForItem:(NSMenuItem *)item previousBorderRect:(NSRect)previousBorderRect {
    NSRect result = NSZeroRect;
    NSSize titleSize = NSZeroSize;

    titleSize = [[self graphicsStyle] menuItemTextSize:[item title]];
    NSImage *img = [item image];
    if(img) {
        titleSize.width += [img size].width;
        titleSize.height = MAX(titleSize.height, kNSMenuItemHeight);
    }

    result.origin = NSMakePoint(6, previousBorderRect.origin.y);
    result.size = titleSize;
    return result;
}

-(NSRect)borderRectFromTitleRect:(NSRect)titleRect {
    NSRect result = NSInsetRect(titleRect, -6, 0);
    result.size.width = [self bounds].size.width;
    return result;
}
 
-(NSImage *)overflowImage {
   return [NSImage imageNamed:@"NSMenuViewDoubleRightArrow"];
}

-(NSRect)overflowRect {
   NSRect bounds = [self bounds];
   NSImage *image = [self overflowImage];
   NSSize size = [image size];
   NSRect rect = NSInsetRect(NSMakeRect(0, 0, size.width, bounds.size.height), -3, 0);

   rect.origin.x = NSMaxX(bounds) - rect.size.width;
   return rect;
}


-(NSUInteger)itemIndexAtPoint:(NSPoint)point {
    NSInteger result = NSNotFound;
    NSRect bounds = [self bounds];
    NSArray *items = [[self menu] itemArray];
    unsigned i, count = [items count];
    NSRect previousBorderRect = NSMakeRect(0, 0, 0, 0);
    BOOL overflow = NO;

    for(i = 0; i < count; ++i) {
        NSMenuItem *item = [items objectAtIndex:i];
        NSRect titleRect = [self titleRectForItem:item previousBorderRect:previousBorderRect];
        NSRect borderRect = [self borderRectFromTitleRect:titleRect];

        if(NSMaxX(borderRect) > NSMaxX(bounds))
            overflow = YES;

        if(NSMouseInRect(point, borderRect, [self isFlipped]))
            result = i;

        previousBorderRect = borderRect;
        previousBorderRect.origin.y += [self heightOfMenuItem:item];
    }

    if(overflow) {
        if(NSMouseInRect(point, [self overflowRect], [self isFlipped]))
            return count;
    }

    return result;
}

-(NSUInteger)selectedItemIndex {
   return _selectedItemIndex;
}

-(void)setSelectedItemIndex:(NSUInteger)itemIndex {
    if (_selectedItemIndex != itemIndex) {
        NSArray *items = [self visibleItemArray];
        id delegate = [[self menu] delegate];

        if ([delegate respondsToSelector:@selector(menu:willHighlightItem:)])
            [delegate menu:[self menu] willHighlightItem:
                (itemIndex < [items count]) ? [items objectAtIndex:itemIndex] : nil];
        _selectedItemIndex=itemIndex;
        [self setNeedsDisplay:YES];
    }
}

-(NSMenuItem *)itemAtSelectedIndex {
   NSArray *items=[self visibleItemArray];

   if(_selectedItemIndex<[items count])
    return [items objectAtIndex:_selectedItemIndex];

   return nil;
}

// This exists because most of AppKit (incl NSView) is not built with ARC, and
// we need something that uses ARC to hold a ref to the window, or it will get
// deallocated prematurely!
-(void)setWindow:(NSWindow *)window {
    _menuWindow = window;
}

#define CENTER_PART_RECT_VERTICALLY(partSize)                                \
{                                                                            \
	NSSize __partSize = (partSize);                                      \
	partRect.origin.y = origin.y + (itemHeight - __partSize.height) / 2; \
	partRect.size.height = __partSize.height;                            \
	partRect.size.width = __partSize.width;                              \
}

#define NSMENUDEBUG 0
-(void)drawRect:(NSRect)rect {
    NSRect bounds = [self bounds];
    NSRect itemArea = boundsToTitleAreaRect(bounds);
    NSArray *items = [self visibleItemArray];
    unsigned i, count = [items count];
    NSPoint origin = itemArea.origin;
    float separatorHeight = [[self graphicsStyle] menuItemSeparatorSize].height; 

    [[self graphicsStyle] drawMenuWindowBackgroundInRect:rect]; // FIXME: MenuBarBackground?	

    for(i = 0; i < count; i++) {
        NSMenuItem *item = [items objectAtIndex:i];
		
        if([item isSeparatorItem]) {
            NSRect separatorRect = NSMakeRect(origin.x, origin.y, NSWidth(itemArea), separatorHeight);
            [[self graphicsStyle] drawMenuSeparatorInRect:separatorRect];
#if NSMENUDEBUG
            [[NSColor yellowColor] setStroke];
            NSFrameRect(separatorRect);
#endif
            origin.y += NSHeight(separatorRect);
        } else {
            NSImage *image = [item image];
            BOOL selected = (i == _selectedItemIndex) ? YES : NO;
            float itemHeight = [self heightOfMenuItem:item];
            NSRect partRect;
            NSSize partSize;
            BOOL showsEnabled = ([item isEnabled] || [item hasSubmenu]);
			
            partRect = NSMakeRect(origin.x, origin.y, itemArea.size.width, itemHeight);
#if NSMENUDEBUG
            [[NSColor redColor] setStroke];
            NSFrameRect(partRect);
#endif
            if(selected)
                [[self graphicsStyle] drawMenuSelectionInRect:partRect enabled:showsEnabled];

            // Draw the gutter and checkmark (if any)
            CENTER_PART_RECT_VERTICALLY([[self graphicsStyle] menuItemGutterSize]);
            if ([item state]) {
                [[self graphicsStyle] drawMenuGutterInRect:partRect];			
                [[self graphicsStyle] drawMenuCheckmarkInRect:partRect enabled:showsEnabled selected:selected];
            }
			
            partRect.origin.x += [[self graphicsStyle] menuItemGutterGap];;
			
            // Draw the image
            if(image) {
                NSRect imageRect;
				
                partRect.origin.x += partRect.size.width;
                CENTER_PART_RECT_VERTICALLY([image size]);
				
                CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
                CGContextSaveGState(ctx);
                CGContextTranslateCTM(ctx, partRect.origin.x, partRect.origin.y);
                if([self isFlipped]) {
                    CGContextTranslateCTM(ctx, 0, partRect.size.height);
                    CGContextScaleCTM(ctx, 1, -1);
                }

                NSRect drawingRect = partRect;
                drawingRect.origin = NSZeroPoint;
                [[self graphicsStyle] drawButtonImage:image inRect:drawingRect enabled:showsEnabled mixed:NO];
                CGContextRestoreGState(ctx);

                partRect.origin.x += IMAGE_TITLE_GAP;
            }
			
            // Draw the title
            partRect.origin.x += partRect.size.width;

            NSAttributedString *atitle = [item attributedTitle];
            if(atitle != nil && [atitle length] > 0) {
                CENTER_PART_RECT_VERTICALLY([atitle size]);
                [[self graphicsStyle] drawAttributedMenuItemText:atitle
                                                          inRect:partRect
                                                         enabled:showsEnabled
                                                        selected:selected];
            } else {
                NSString *title = [item title];
                CENTER_PART_RECT_VERTICALLY([[self graphicsStyle] menuItemTextSize:title]);
                [[self graphicsStyle] drawMenuItemText:title
                                                inRect:partRect
                                               enabled:showsEnabled
                                              selected:selected];
            }
			
            // Draw the key equivalent
            if([[item keyEquivalent] length] != 0) {
                NSString *keyString = [item _keyEquivalentDescription];
                NSSize branchArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
                NSSize keyEquivalentSize = [[self graphicsStyle] menuItemTextSize:keyString];
				
                partRect.origin.x = origin.x + NSWidth(itemArea) - branchArrowSize.width - keyEquivalentSize.width;
                CENTER_PART_RECT_VERTICALLY(keyEquivalentSize);
                [[self graphicsStyle] drawMenuItemText:keyString
                                                inRect:partRect
                                               enabled:showsEnabled
                                              selected:selected];
            }
			
            // Draw the submenu arrow
            if([item hasSubmenu] && [[item submenu] numberOfItems] > 0) {
                NSSize branchArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
                partRect.origin.x = origin.x + NSWidth(itemArea) - branchArrowSize.width;
                partRect.size.width = branchArrowSize.width;
                CENTER_PART_RECT_VERTICALLY(branchArrowSize);
                [[self graphicsStyle] drawMenuBranchArrowInRect:partRect enabled:showsEnabled selected:selected];
            }

#if NSMENUDEBUG
            [[NSColor yellowColor] setStroke];
            NSFrameRect(partRect);
#endif

            origin.y += itemHeight;
        }
    }
}

/*
 * Tracking: from a menu bar (NSMenuBarView), a context menu or a pop-up
 * menu, the same loop over the stack of open menu views.
 */
const float kMenuInitialClickThreshold = .3f;
const float kMouseMovementThreshold = .001f;
#if 0
#define MENUDEBUG(...) NSLog(__VA_ARGS__)
#else
#define MENUDEBUG(...)
#endif

-(NSMenuItem *)trackForEvent:(NSEvent *)event {
	return [self _trackForEvent:event selecting:NSNotFound startedOpen:NO];
}

/* startedOpen: the menu was opened by the keyboard or by a program, not by
   a press now held: it stays open until an item is chosen or it is left,
   and the pointer is not looked at until it moves. selecting: the item
   selected at first, or NSNotFound. */
-(NSMenuItem *)_trackForEvent:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
	NSMenuItem *item=nil;
	
	enum {
		STATE_FIRSTMOUSEDOWN,
		STATE_MOUSEDOWN,
		STATE_MOUSEUP,
		STATE_EXIT
	} state=STATE_FIRSTMOUSEDOWN;
	
	// Get the menu management ball rolling
	NSPoint point=[event locationInWindow];
	NSPoint firstPoint=point;
	NSTimeInterval firstTimestamp = [event timestamp];
	
	// Cascading menus we manage will be pushed and popped on the viewStack
	NSMutableArray *viewStack=[NSMutableArray array];
	
	// Make sure we can put things back the way we found them
	BOOL oldAcceptsMouseMovedEvents = [[self window] acceptsMouseMovedEvents];
	
	// But while we're dealing with menus we want to track the mouse...
	[[self window] setAcceptsMouseMovedEvents:YES];
	
	// Each menu is brought up to date as it opens (NSMenuWindow)
	
	// And we, of course, are first on the stack
	[viewStack addObject:self];
	
	int keyboardNavigationAction = kNSMenuKeyboardNavigationNone;

	if (startedOpen) {
		[self setSelectedItemIndex:selecting];
		keyboardNavigationAction = kNSMenuKeyboardNavigationStart;
		state = STATE_MOUSEUP;
	}
	
	BOOL cancelled = NO;
	// A release over no item closes the menus only after a new press, or
	// once the pointer has been over an item: a press held and released
	// where the menu opened leaves it open
	BOOL pressedSinceOpen = NO, overItemOnce = NO;
	
	MENUDEBUG(@"entering outer loop");
	
	// Start tracking the mouse movements and clicks
	do {
		int                count=[viewStack count];
		NSScreen          *screen=[self _screenForPoint:[[event window] convertBaseToScreen:point]];
		
		point=[[event window] convertBaseToScreen:point];
		
		// We've not pushed any views yet so the screen is where our window is
		if(count==1) {
			screen=[self _screenForPoint:point];
		}
                MENUDEBUG(@"INITIAL EVENT POINT IS %@ (count == %d, screen == %@)",
                        NSStringFromPoint(point), count, screen);
		
        if (keyboardNavigationAction == kNSMenuKeyboardNavigationNone) {
            // We're not current dealing with a keyboard event
            // Take a look at the visible menu stack (we're within a big loop so views can come and
            // go and the mouse can wander all over) deepest first
            while(--count>=0){
                
                // get the deepest one
                NSMenuView *checkView=[viewStack objectAtIndex:count];
                
                // And find out where the mouse is relative to it
                NSPoint     checkPoint=[[checkView window] convertScreenToBase:point];
                MENUDEBUG(@"1 CHECKPOINT IS %@", NSStringFromPoint(checkPoint));
                
                checkPoint=[checkView convertPoint:checkPoint fromView:nil];
                MENUDEBUG(@"2 CHECKPOINT IS %@", NSStringFromPoint(checkPoint));
                
                // If it's inside the menu view
                if(NSMouseInRect(checkPoint,[checkView bounds],[checkView isFlipped])){
                    
                    MENUDEBUG(@"found a menu: %@", checkView);
                    
                    // Which item is the cursor on top of?
                    unsigned itemIndex=[checkView itemIndexAtPoint:checkPoint];

                    MENUDEBUG(@"found an item index: %u", itemIndex);

                    // If it's not the currently selected item
                    if(itemIndex!=[checkView selectedItemIndex]){
                        NSMenuView *branch;
                        
                        // This looks like it's dealing with pushed cascading menu
                        // views that are no longer needed because the user has moved
                        // on - so pop them all off.
                        while (count+1<[viewStack count]) {
                            NSView* view = [viewStack lastObject];
                            MENUDEBUG(@"popping cascading view: %@", view);
                            [[view window] close];
                            [viewStack removeLastObject];
                        }
                        
                        // And now select the new item
                        MENUDEBUG(@"about to setSelectedItemIndex");
                        [checkView setSelectedItemIndex:itemIndex];
                        
                        // If it's got a cascading menu then push that on the stack
                        if((branch=[checkView viewAtSelectedIndexPositionOnScreen:screen])!=nil) {
                            MENUDEBUG(@"adding a new cascading view: %@", branch);
                            [viewStack addObject:branch];
                        }
                    }
                    // And bail out of the while loop - we're in the right place
                    break;
                } else {
                    // We've wandered off the menu so don't show anything selected if it's the deepest
                    // visible view
                    if (checkView == [viewStack lastObject]) {
                        MENUDEBUG(@"clearing selection in view: %@", checkView);
                        // The mouse is outside of the top menu - be sure no item is selected anymore
                        [checkView setSelectedItemIndex:NSNotFound];
                    }
                }
            }
            MENUDEBUG(@"exited --count>=0 loop");
            
            // Looks like we've popped everything so nothing can be selected
            if(count<0){
                MENUDEBUG(@"clearing all selection");
                [[viewStack lastObject] setSelectedItemIndex:NSNotFound];
            }
        }
		
		// Let's take a look at what's come in on the event queue
                MENUDEBUG(@"getting next window event for %@", [self window]);
		// Presses are taken too, so a click that closes the menus goes no
		// further; the right and other buttons drive menus as the left one
		event=[[self window] nextEventMatchingMask:NSLeftMouseDownMask|NSLeftMouseUpMask|
			NSRightMouseDownMask|NSRightMouseUpMask|NSOtherMouseDownMask|NSOtherMouseUpMask|
			NSMouseMovedMask|NSLeftMouseDraggedMask|NSRightMouseDraggedMask|NSOtherMouseDraggedMask|
			NSKeyDownMask|NSAppKitDefinedMask];
		BOOL isMouseUp = ([event type]==NSLeftMouseUp || [event type]==NSRightMouseUp || [event type]==NSOtherMouseUp);
		BOOL isMouseDown = ([event type]==NSLeftMouseDown || [event type]==NSRightMouseDown || [event type]==NSOtherMouseDown);
		if (isMouseDown)
			pressedSinceOpen = YES;
		
		if (keyboardNavigationAction != kNSMenuKeyboardNavigationNone) {
			// We didn't enter the mouse handling loop that predecrements count - so do it here...
			count--;
		}
		// Reset the keyboard navigation state
		keyboardNavigationAction = kNSMenuKeyboardNavigationNone;
		
        // Sometimes we can get key events with no characters (if the user
        // has invoked an accelerator while the menu is open for example)
		if ([event type] == NSKeyDown && [[event characters] length] > 0) {

			NSString* chars = [event characters];
			unichar ch = [chars characterAtIndex: 0];
			switch (ch) {
				case NSUpArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationUp;
					break;
				case NSDownArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationDown;
					break;
				case NSLeftArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationLeft;
					break;
				case NSRightArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationRight;
					break;
					
				case '\r': // Return = select the current item and exit the loop
				case 3: // Enter
					MENUDEBUG(@"Selecting current item and exit");
					// The keyboard's item, not one the mouse passed over
					item = [[viewStack lastObject] itemAtSelectedIndex];
					state = STATE_EXIT;
					break;
				case 27: // Escape = pop unless we're done then it's cancel
				{
					// Menus opened with the pointer close at once; from the
					// keyboard (Control-F2) one level at a time
					if (!startedOpen) {
						cancelled = YES;
					} else if ([viewStack count] > 1) {
						NSView* view = [viewStack lastObject];
						MENUDEBUG(@"popping cascading view: %@", view);
						[[view window] close];
						[viewStack removeLastObject];
					}	else {
						MENUDEBUG(@"Cancelling");
						cancelled = YES;
					}
				}
					break;
				default:
					keyboardNavigationAction = kNSMenuKeyboardNavigationLetter;
					break;
			}
			
			NSMenuView* activeMenuView = [viewStack lastObject];

			BOOL ignoreEnabledState = NO;
			if ([viewStack count] == 1 && [activeMenuView _isMenuBar]) {
				// For some reason main menu items are disabled - even though they work fine...
				ignoreEnabledState = YES;
				// we're navigating the top menu which has opposite semantics than a regular menu
				switch (keyboardNavigationAction) {
					case kNSMenuKeyboardNavigationDown:
						keyboardNavigationAction = kNSMenuKeyboardNavigationRight;
						break;
					case kNSMenuKeyboardNavigationUp:
						keyboardNavigationAction = kNSMenuKeyboardNavigationLeft;
						break;
					case kNSMenuKeyboardNavigationLeft:
						keyboardNavigationAction = kNSMenuKeyboardNavigationUp;
						break;
					case kNSMenuKeyboardNavigationRight:
						keyboardNavigationAction = kNSMenuKeyboardNavigationDown;
						break;
				}
			}
			
			switch (keyboardNavigationAction) {
				case kNSMenuKeyboardNavigationUp:
				{
					MENUDEBUG(@"Up...");
					
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search up and wrapping around the bottom
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == 0 ? [items count] - 1 : oldIndex - 1;
					}
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ([item isSeparatorItem] == NO &&
							((ignoreEnabledState || [item isEnabled]) || [item hasSubmenu])) {
							MENUDEBUG(@"selecting item = %@", item);
							[activeMenuView setSelectedItemIndex: newIndex];
							found = YES;
						} else {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == 0) {
								newIndex = [items count] - 1;
							} else {
								newIndex --;
							}
						}
					}
				}
					break;
				case kNSMenuKeyboardNavigationDown:
				{
					MENUDEBUG(@"Down...");
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search down and wrapping around to the top
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == [items count] -1 ? 0 : oldIndex + 1;
					}
					
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ([item isSeparatorItem] == NO &&
							((ignoreEnabledState || [item isEnabled]) || [item hasSubmenu])) {
							MENUDEBUG(@"selecting item: %u", item);
							[activeMenuView setSelectedItemIndex: newIndex];
							found = YES;
						} else {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == [items count] - 1) {
								newIndex = 0;
							} else {
								newIndex++;
							}
						}
					}
				}
					break;
				case kNSMenuKeyboardNavigationLeft:
					MENUDEBUG(@"Left...");
					if ([viewStack count] > 1) {
						NSView* view = [viewStack lastObject];
						MENUDEBUG(@"popping cascading view: %@", view);
						[[view window] close];
						[viewStack removeLastObject];
					}	
					break;
				case kNSMenuKeyboardNavigationRight:
                {
						MENUDEBUG(@"Right...");
					NSMenuView *branch = nil;
					// If there's a submenu at the current  selected index
					if((branch=[activeMenuView viewAtSelectedIndexPositionOnScreen:screen])!=nil) {
						MENUDEBUG(@"adding a new cascading view: %@", branch);
						[viewStack addObject:branch];
						// Opened from the keyboard: its first usable item is selected
						NSArray *branchItems = [branch itemArray];
						for (NSUInteger i = 0; i < [branchItems count]; i++) {
							NSMenuItem *candidate = [branchItems objectAtIndex: i];
							if (![candidate isSeparatorItem] && ([candidate isEnabled] || [candidate hasSubmenu])) {
								[branch setSelectedItemIndex: i];
								break;
							}
						}
					} else {
						// We'll pop it - they're trying to navigate to the next menu most likely
						if ([viewStack count] > 1) {
							NSView* view = [viewStack lastObject];
							MENUDEBUG(@"popping cascading view: %@", view);
							[[view window] close];
							[viewStack removeLastObject];
						}
					}
                }
					break;
				case kNSMenuKeyboardNavigationLetter:
				{
					MENUDEBUG(@"Letter...");
					NSString *letterString = [[NSString stringWithCharacters: &ch length: 1] uppercaseString];
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search down and wrapping around to the top
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == [items count] -1 ? 0 : oldIndex + 1;
					}
					
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ((ignoreEnabledState || [item isEnabled] == YES) || [item hasSubmenu] == YES) {
							NSRange range = [[item title] rangeOfString: letterString];
							if (range.location != NSNotFound) {
								MENUDEBUG(@"selecting item: %u", item);
								[activeMenuView setSelectedItemIndex: newIndex];
								found = YES;
							}
						}
						if (!found) {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == [items count] - 1) {
								newIndex = 0;
							} else {
								newIndex++;
							}
						}
					}
				}
					break;
			}
		}
		// We use this special AppKitDefined event to let the menu respond to the app deactivation - it *has*
		// to be passed through the event system, unfortunately
		if ([event type] == NSAppKitDefined) {
			if ([event subtype] == NSApplicationDeactivated) {
				MENUDEBUG(@"NSApplicationDeactivated");
				cancelled = YES;
			}
		}
		
		if (cancelled == NO && [event type] != NSAppKitDefined && [event type] != NSKeyDown) {
			
			// looks like we can keep rolling
			
			point=[event locationInWindow];
                        MENUDEBUG(@"1 SECOND POINT is now %@", NSStringFromPoint(point));
                        point = [[event window] convertScreenToBase:point];
                        MENUDEBUG(@"2 SECOND POINT is now %@", NSStringFromPoint(point));
                        point.y += [screen frame].size.height;
                        MENUDEBUG(@"3 SECOND POINT is now %@", NSStringFromPoint(point));

			// Don't test for "== 0." - we tend to receive some delta with some .000000... values while the mouse doesn't move
			BOOL mouseMoved = ([event type] != NSAppKitDefined) &&
								(fabs([event deltaX]) > kMouseMovementThreshold || fabs([event deltaY]) > kMouseMovementThreshold);
			
			NSMenuView * activeView = nil;

			// We may not have a menuview here - so be cautious - and we may have added a cascading menu
			// so lastObject is also not the right thing to look at - we need to look at the menuview found in
			// the preceeding block (if there was one found - the user could have moused somewhere else entirely
			// remember)
			if (count >= 0) {
				activeView = [viewStack objectAtIndex: count];
                                MENUDEBUG(@"activeView == %@ at index %d", activeView, count);
			}
			
                        MENUDEBUG(@"switch(%d)", state);
			switch(state){
				case STATE_FIRSTMOUSEDOWN:
					// Let's take a look at the item under the cursor (if there is one)
					item=[activeView itemAtSelectedIndex];
                                        MENUDEBUG(@"itemAtSelectedIndex == %@ for index %d", item, _selectedItemIndex);
					if (item != nil)
						overItemOnce = YES;
					
					if(isMouseUp) {
						// The menu is really active after a mouse up (which means the menu will stay sticky)...
						// The timestamp is to avoid false clicks - make sure there's a delay so the user can
						if ([event timestamp] - firstTimestamp > kMenuInitialClickThreshold &&
							[viewStack count]==1 && [item isEnabled]) {
							MENUDEBUG(@"Handling selected item - exiting");
							state=STATE_EXIT;
						} else {
							MENUDEBUG(@"mouse up - continuing");
							state=STATE_MOUSEUP;
						}
					} else if(isMouseDown || mouseMoved) {
						// .. Or a mouse down (second click after the sticky menu) or a real move
						state=STATE_MOUSEDOWN;
					}
					break;
					
				default:
					item=[activeView itemAtSelectedIndex];
					if (item != nil)
						overItemOnce = YES;
					if(isMouseUp){
						MENUDEBUG(@"mouseUp on item: %@", item);
						/* Released over a disabled item or one with a submenu
						   (a menu title too), the menu stays open. */
						if((item == nil && (pressedSinceOpen || overItemOnce)) ||
						   ([item isEnabled] && ![item hasSubmenu])) {
							MENUDEBUG(@"mouse up - exiting because of many possible reasons...");
							state=STATE_EXIT;
						} else {
							MENUDEBUG(@"mouse up");
							state=STATE_MOUSEUP;
						}
					}
					break;
			}
                        point = [event locationInWindow];
                        MENUDEBUG(@"reset point!");
		}
	}while(cancelled == NO && state!=STATE_EXIT);
	
	MENUDEBUG(@"done with the event loop");
	
	// If we've got a menu still visible
	if(item == nil && [viewStack count]>0) {
		// Get the selected item
		item=[[viewStack lastObject] itemAtSelectedIndex];
		MENUDEBUG(@"got the selected item at the top most menu view: %@", item);
	}
	
	MENUDEBUG(@"removing the visible menu views");
	while([viewStack count]>1){
                NSView *v = (NSView *)[viewStack lastObject];
                NSWindow *w = [v window];
                MENUDEBUG(@"closing window %@ for view %@", w, v);
		[[(NSView *)[viewStack lastObject] window] close];
		[viewStack removeLastObject];
	}
	[viewStack removeLastObject];
	
	_selectedItemIndex=NSNotFound;
	[[self window] setAcceptsMouseMovedEvents:oldAcceptsMouseMovedEvents];
	[self setNeedsDisplay:YES];
	
	return ([item isEnabled] && ![item hasSubmenu])?item:(NSMenuItem *)nil;
}

-(void)mouseDown:(NSEvent *)event {
   [self _trackAndPerform:event selecting:NSNotFound startedOpen:NO];
}

-(void)_trackFromKeyboard {
   [self _trackAndPerform:[NSApp currentEvent] selecting:0 startedOpen:YES];
}

/* Tracks, then sends the chosen item's action; the item, or nil. */
-(NSMenuItem *)_trackAndPerform:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
   NSMenuItem *item=[self _trackMenus:event selecting:selecting startedOpen:startedOpen];

   if(item!=nil)
    [[item menu] _sendActionOfItem:item];
   return item;
}

/* Tracks with the pointer held by this menu, between the tracking
   notifications; the item chosen, or nil. */
-(NSMenuItem *)_trackMenus:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
   BOOL        didAccept=[[self window] acceptsMouseMovedEvents];
   NSMenuItem *item;

   [[self window] setAcceptsMouseMovedEvents:YES];
   /* With the new server, the menu holding the pointer while its menus
      are open has clicks outside them too (they close them). */
   NSNotificationCenter *center=[NSNotificationCenter defaultCenter];

   [center postNotificationName:NSMenuDidBeginTrackingNotification object:[self menu]];
   if(_CGUsesSkyLight())
    SLSCapturePointer([[self window] _skyLightWindowID]);
   item=[self _trackForEvent:event selecting:selecting startedOpen:startedOpen];
   if(_CGUsesSkyLight())
    SLSCapturePointer(0);
   [[self window] setAcceptsMouseMovedEvents:didAccept];
   [center postNotificationName:NSMenuDidEndTrackingNotification object:[self menu]];
   return item;
}

@end
