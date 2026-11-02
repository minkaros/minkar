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

#import <AppKit/AppKit.h>
#import <AppKit/NSGraphicsStyle.h>

@interface NSMenu(NSMenuViews)
-(NSString *)_name;
-(BOOL)_sendActionOfItem:(NSMenuItem *)item;
-(NSMenuItem *)_popUpAtScreenPoint:(NSPoint)screenPoint positioning:(NSMenuItem *)positioned
    event:(NSEvent *)event startedOpen:(BOOL)startedOpen minimumWidth:(CGFloat)minimumWidth
    flips:(BOOL)flips perform:(BOOL)perform;
@end

/* An item's height in the menu bar and in menus. SHORTCUT(Phase 5): the
 * look of SystemUIServer's menus, kept until Gienah's design; taken in
 * WS4b. */
#define kNSMenuItemHeight	22

@interface NSMenuView : NSView {
    NSWindow *_menuWindow;
    NSUInteger _selectedItemIndex;
    NSMutableArray *_visibleArray;
}

-initWithMenu:(NSMenu *)menu;
-(NSArray *)itemArray;
-(NSArray *)visibleItemArray;
-(NSSize)sizeForMenuItemImage:(NSMenuItem *)item;
-(NSSize)sizeForMenuItems:(NSArray *)items;
-(void)positionBranchForSelectedItem:(NSWindow *)branch screen:(NSScreen *)screen;
-(NSMenuView *)viewAtSelectedIndexPositionOnScreen:(NSScreen *)screen;
-(unsigned)itemIndexAtPoint:(NSPoint)point;
-(NSUInteger)selectedItemIndex;
-(void)setSelectedItemIndex:(NSUInteger)itemIndex;
-(void)mouseDown:(NSEvent *)event;
-(NSMenuItem *)trackForEvent:(NSEvent *)event;
/* NO here, YES for the menu bar, whose items run left to right. */
-(BOOL)_isMenuBar;
/* Tracks the menus from here and sends the chosen item's action (see
   _trackForEvent:selecting:startedOpen:); the item chosen, or nil. */
-(NSMenuItem *)_trackAndPerform:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen;
-(NSMenuItem *)_trackForEvent:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen;
-(NSMenuItem *)_trackMenus:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen;
/* Control-F2: from the keyboard, the first item selected. */
-(void)_trackFromKeyboard;
/* How far below the menu's top the item's top is, in points. */
-(CGFloat)_topOfItemAtIndex:(NSUInteger)index;
-(NSMenuItem *)itemAtSelectedIndex;
-(void)setWindow:(NSWindow *)window;

@end
